// Off-device unit tests for the Geode-free core.
// Build: see tests/run_tests.sh
#include "Core/FrameBucket.hpp"
#include "Core/MeasurementManager.hpp"
#include "Core/WindowSolver.hpp"
#include "Methods/DirectTickMethod.hpp"
#include "Recording/InputRecorder.hpp"
#include "Storage/RunStorage.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>

using namespace afc;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static void buckets() {
    struct { int n; Bucket b; } t[] = {{15,Bucket::B10_15},{10,Bucket::B10_15},{9,Bucket::B7_9},{7,Bucket::B7_9},
        {6,Bucket::B4_6},{4,Bucket::B4_6},{3,Bucket::B3},{2,Bucket::B2},{1,Bucket::B1},{0,Bucket::None},{31,Bucket::B10_15}};
    for (auto& x : t) CHECK(classify(x.n) == x.b);
    Measurement m; m.exact = 5; m.validity = Validity::Valid;
    CHECK(formatMeasurement(m, true, true) == "5 / 4-6");
    CHECK(formatMeasurement(m, true, false) == "5");
    CHECK(formatMeasurement(m, false, true) == "4-6");
    m.exact = 3; CHECK(formatMeasurement(m, true, true) == "3");
    m.exact = 21; m.capped = true; CHECK(formatMeasurement(m, true, true) == "21+ / 10-15");
    m.validity = Validity::Unavailable; CHECK(formatMeasurement(m, true, true) == "UNAVAILABLE");
}

static void solver() {
    // Survivable offsets are exactly [-2, +3] -> window 6.
    auto r = solveWindow(15, [](int d) { return (d >= -2 && d <= 3) ? Probe::Survives : Probe::Dies; });
    CHECK(r.validity == Validity::Valid && r.exact == 6 && r.earliest == -2 && r.latest == 3 && !r.capped);
    CHECK(r.probesRun + r.probesSkipped == 31);
    // Only offset 0 survives -> window 1.
    r = solveWindow(15, [](int d) { return d == 0 ? Probe::Survives : Probe::Dies; });
    CHECK(r.exact == 1);
    // Everything survives -> capped at 2*range+1.
    r = solveWindow(10, [](int) { return Probe::Survives; });
    CHECK(r.exact == 21 && r.capped);
    // Offset 0 dies -> limitation, no number invented.
    r = solveWindow(10, [](int) { return Probe::Dies; });
    CHECK(r.validity == Validity::MethodLimitation && r.exact == -1);
    // Probe failure propagates.
    r = solveWindow(10, [](int d) { return d == 0 ? Probe::Survives : (d == 2 ? Probe::Failed : Probe::Survives); });
    CHECK(r.validity == Validity::MethodLimitation);
}

static void search() {
    auto& mm = MeasurementManager::get();
    mm.clear(); mm.setCapacity(50);
    int vals[] = {8, 5, 3, 1, 12};
    for (int i = 0; i < 5; ++i) {
        Measurement m; m.inputIndex = i + 1; m.exact = vals[i]; m.validity = Validity::Valid;
        m.player = (i % 2) + 1; m.method = Method::StateProbe; m.kind = Kind::Window;
        mm.add(m);
    }
    Query q; std::string err;
    CHECK(parseQuery("3", q, &err)); CHECK(mm.search(q).size() == 1);
    CHECK(parseQuery("4-6", q, &err)); CHECK(mm.search(q).size() == 1 && mm.search(q)[0].exact == 5);
    CHECK(parseQuery("7\xE2\x80\x93" "9", q, &err)); CHECK(mm.search(q).size() == 1 && mm.search(q)[0].exact == 8);
    CHECK(parseQuery("10-15", q, &err)); CHECK(mm.search(q).size() == 1 && mm.search(q)[0].exact == 12);
    CHECK(parseQuery("p2", q, &err)); CHECK(mm.search(q).size() == 2);
    CHECK(parseQuery("#3", q, &err)); CHECK(mm.search(q).size() == 1 && mm.search(q)[0].exact == 3);
    CHECK(parseQuery("m1", q, &err)); CHECK(mm.search(q).empty());
    CHECK(!parseQuery("banana", q, &err));
    auto c = mm.bucketCounts(Method::StateProbe, 0);
    CHECK(c[0] == 1 && c[1] == 1 && c[2] == 1 && c[3] == 1 && c[4] == 0 && c[5] == 1);
    mm.setCapacity(8);
    for (int i = 0; i < 30; ++i) { Measurement m; m.exact = 1; m.validity = Validity::Valid; mm.add(m); }
    CHECK(mm.all().size() == 8);   // bounded
}

static void directMethod() {
    auto& rec = InputRecorder::get();
    auto& mm = MeasurementManager::get(); mm.clear();
    rec.newAttempt(123, "Test", 240, false);
    DirectTickMethod m1;
    int ticks[] = {100, 108, 109, 130};
    for (int t : ticks) {
        InputEvent e; e.tick = t; e.down = true;
        int idx = rec.push(e);
        m1.onInput(e, idx, 123);
        InputEvent r; r.tick = t + 2; r.down = false; rec.push(r);   // release: no measurement
    }
    auto& all = mm.all();
    CHECK(all.size() == 4);
    CHECK(all[0].validity == Validity::Unavailable && all[0].exact == -1);
    CHECK(all[1].exact == 8 && all[2].exact == 1 && all[3].exact == 21);
    CHECK(all[3].inputIndex == 4);
}

static void runFormat() {
    auto& rec = InputRecorder::get();
    rec.newAttempt(77, "Level \"Q\"\n", 240, true);
    CHECK(rec.start());
    for (int i = 0; i < 5; ++i) {
        InputEvent e; e.tick = 10 * i; e.down = true; e.player = (i % 2) + 1; e.x = 1.5f * i; rec.push(e);
        e.tick += 3; e.down = false; rec.push(e);
    }
    CHECK(rec.stop(100));
    CHECK(rec.run().pressCount() == 5 && rec.run().inputs.size() == 10);

    auto tmp = std::filesystem::temp_directory_path() / "afc_test_dir";
    std::filesystem::remove_all(tmp);
    RunStorage st(tmp);
    std::string name, err;
    CHECK(st.save(rec.run(), name, err));
    RecordedRun back;
    CHECK(st.load(name, back, err));
    CHECK(back.levelID == 77 && back.levelName == "Level \"Q\"\n" && back.inputs.size() == 10 &&
          back.platformer && back.durationTicks == 100 && back.inputs[2].player == 2);
    CHECK(st.list().size() == 1);
    // corrupted file
    { std::ofstream f(tmp / "runs" / "bad.json"); f << "{not json"; }
    CHECK(!st.load("bad.json", back, err));
    CHECK(st.list().size() == 1);              // corrupted skipped
    CHECK(!st.load("../x.json", back, err));   // path traversal rejected
    // invalid structure: unordered ticks
    RecordedRun bad = rec.run(); std::swap(bad.inputs[0], bad.inputs[5]);
    CHECK(!bad.validate(err));
    CHECK(!st.save(bad, name, err));
    // future version
    json::Value v = rec.run().toJson();
    for (auto& kv : v.o) if (kv.first == "version") kv.second.n = 99;
    CHECK(!RecordedRun::fromJson(v, back, err));
    CHECK(st.remove(name, err));
    std::filesystem::remove_all(tmp);
}

int main() {
    buckets(); solver(); search(); directMethod(); runFormat();
    std::printf(fails ? "%d FAILED\n" : "all tests passed\n", fails);
    return fails ? 1 : 0;
}
