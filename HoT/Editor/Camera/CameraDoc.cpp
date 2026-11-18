#include "HoT/Editor/Camera/CameraDoc.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include "HoT/Editor/VectorMath.h"

namespace HoT::Editor::Camera {

using namespace VectorMath;

static constexpr uint32_t kMagic = 0x3032444D;
static constexpr uint32_t kVersion = 264;
static constexpr uint32_t kHeaderSize = 0x130;
static constexpr uint32_t kSequenceSize = 0x40;
static constexpr uint32_t kBoneSize = 0x58;
static constexpr uint32_t kCameraSize = 0x64;
static constexpr int32_t kLengthSteps = 48;

bool IsSpline(uint16_t interp) {
    return interp == INTERP_BEZIER || interp == INTERP_HERMITE;
}

C3Vector DirToWorld(const Doc& doc, const C3Vector& local) {
    float c = cosf(doc.m_facing);
    float s = sinf(doc.m_facing);

    return { local.x * c - local.y * s, local.x * s + local.y * c, local.z };
}

C3Vector DirToLocal(const Doc& doc, const C3Vector& world) {
    float c = cosf(doc.m_facing);
    float s = sinf(doc.m_facing);

    return { world.x * c + world.y * s, -world.x * s + world.y * c, world.z };
}

// CGCamera::SetModelCamera translates to the origin then rotates the model matrix around Z, so local points rotate first.
C3Vector ToWorld(const Doc& doc, const C3Vector& local) {
    return Add(DirToWorld(doc, local), doc.m_origin);
}

C3Vector ToLocal(const Doc& doc, const C3Vector& world) {
    return DirToLocal(doc, Subtract(world, doc.m_origin));
}

static C3Vector Bezier(const C3Vector& p0, const C3Vector& p1, const C3Vector& p2, const C3Vector& p3, float t) {
    float u = 1.0f - t;
    float b0 = u * u * u;
    float b1 = 3.0f * t * u * u;
    float b2 = 3.0f * t * t * u;
    float b3 = t * t * t;

    return {
        p0.x * b0 + p1.x * b1 + p2.x * b2 + p3.x * b3,
        p0.y * b0 + p1.y * b1 + p2.y * b2 + p3.y * b3,
        p0.z * b0 + p1.z * b1 + p2.z * b2 + p3.z * b3,
    };
}

// The point at ratio t of the segment after key k.
static C3Vector SegmentPoint(const std::vector<Key>& keys, uint16_t interp, size_t k, float t) {
    const Key& k0 = keys[k];
    const Key& k1 = keys[k + 1];

    switch (interp) {
        case INTERP_NONE:
            return k0.m_value;
        case INTERP_LINEAR:
            return Add(k0.m_value, Scale(Subtract(k1.m_value, k0.m_value), t));
        default:
            // Hermite is written as the same cubic, see Build.
            return Bezier(k0.m_value, Add(k0.m_value, k0.m_out), Add(k1.m_value, k1.m_in), k1.m_value, t);
    }
}

C3Vector Evaluate(const Doc& doc, int32_t track, float time) {
    const std::vector<Key>& keys = doc.m_keys[track];

    if (keys.empty()) {
        return {};
    }

    if (keys.size() == 1 || time <= static_cast<float>(keys.front().m_time)) {
        return keys.front().m_value;
    }

    if (time >= static_cast<float>(keys.back().m_time)) {
        return keys.back().m_value;
    }

    size_t low = 0;
    size_t high = keys.size() - 1;

    while (high - low > 1) {
        size_t mid = (low + high) / 2;

        if (static_cast<float>(keys[mid].m_time) <= time) {
            low = mid;
        } else {
            high = mid;
        }
    }

    float t0 = static_cast<float>(keys[low].m_time);
    float t1 = static_cast<float>(keys[low + 1].m_time);
    float ratio = t1 > t0 ? (time - t0) / (t1 - t0) : 0.0f;

    return SegmentPoint(keys, doc.m_interp[track], low, ratio);
}

C3Vector EvaluateWorld(const Doc& doc, int32_t track, float time) {
    C3Vector local = Evaluate(doc, track, time);

    return track == TRACK_ROLL ? local : ToWorld(doc, local);
}

static float SegmentTime(const std::vector<Key>& keys, size_t from, size_t to) {
    float dt = static_cast<float>(keys[to].m_time) - static_cast<float>(keys[from].m_time);

    return dt > 0.0f ? dt : 1.0f;
}

static float SignedLength(const C3Vector& value, bool scalar) {
    return scalar ? fabsf(value.x) : Length(value);
}

// The other handle of a smooth key, so the speed leaving equals the speed arriving.
static C3Vector MatchedHandle(const std::vector<Key>& keys, size_t k, bool fromOut, const C3Vector& handle) {
    float timeFrom = fromOut ? SegmentTime(keys, k, k + 1) : SegmentTime(keys, k - 1, k);
    float timeTo = fromOut ? SegmentTime(keys, k - 1, k) : SegmentTime(keys, k, k + 1);

    return Scale(handle, -timeTo / timeFrom);
}

// Speed through point k of a curve through values at times, in units per millisecond.
// Limited to 1.5 times the slower neighbouring segment so a handle never reaches past half its segment, which stops loops and overshoot.
// stopOnReverse makes it zero where the curve turns back, so it cannot swing past a key.
static C3Vector AutoVelocity(const std::vector<C3Vector>& values, const std::vector<uint32_t>& times, size_t k, bool stopOnReverse) {
    size_t count = values.size();
    auto slope = [&](size_t from) {
        float dt = static_cast<float>(times[from + 1]) - static_cast<float>(times[from]);
        return Scale(Subtract(values[from + 1], values[from]), 1.0f / (dt > 0.0f ? dt : 1.0f));
    };

    if (count < 2) {
        return {};
    }

    if (k == 0) {
        return slope(0);
    }

    if (k == count - 1) {
        return slope(k - 1);
    }

    C3Vector left = slope(k - 1);
    C3Vector right = slope(k);
    float span = static_cast<float>(times[k + 1]) - static_cast<float>(times[k - 1]);
    C3Vector velocity = Scale(Subtract(values[k + 1], values[k - 1]), 1.0f / (span > 0.0f ? span : 1.0f));

    if (stopOnReverse && Dot(left, right) <= 0.0f) {
        return {};
    }

    float limit = 1.5f * std::min(Length(left), Length(right));
    float speed = Length(velocity);

    return speed > limit && speed > 0.0f ? Scale(velocity, limit / speed) : velocity;
}

static void RebuildHandles(std::vector<Key>& keys, bool stopOnReverse) {
    size_t count = keys.size();
    std::vector<C3Vector> values(count);
    std::vector<uint32_t> times(count);

    for (size_t k = 0; k < count; k++) {
        values[k] = keys[k].m_value;
        times[k] = keys[k].m_time;
    }

    for (size_t k = 0; k < count; k++) {
        Key& key = keys[k];

        if (count < 2 || key.m_mode == TANGENT_STOP) {
            key.m_in = {};
            key.m_out = {};
            continue;
        }

        if (key.m_mode == TANGENT_AUTO) {
            C3Vector velocity = AutoVelocity(values, times, k, stopOnReverse);
            key.m_out = k + 1 < count ? Scale(velocity, SegmentTime(keys, k, k + 1) / 3.0f) : C3Vector{};
            key.m_in = k > 0 ? Scale(velocity, -SegmentTime(keys, k - 1, k) / 3.0f) : C3Vector{};
            continue;
        }

        if (key.m_mode == TANGENT_SMOOTH && k > 0 && k + 1 < count) {
            key.m_in = MatchedHandle(keys, k, true, key.m_out);
        }
    }
}

static bool KeyTimeLess(const Key& a, const Key& b) {
    return a.m_time < b.m_time;
}

void Normalize(Doc& doc, int32_t track) {
    std::vector<Key>& keys = doc.m_keys[track];
    std::stable_sort(keys.begin(), keys.end(), KeyTimeLess);

    for (size_t k = 1; k < keys.size(); k++) {
        if (keys[k].m_time <= keys[k - 1].m_time) {
            keys[k].m_time = keys[k - 1].m_time + 1;
        }
    }

    if (track == TRACK_ROLL) {
        for (Key& key : keys) {
            key.m_value.y = key.m_value.z = 0.0f;
            key.m_in.y = key.m_in.z = 0.0f;
            key.m_out.y = key.m_out.z = 0.0f;
        }
    }

    // Roll keeps to the values it was given, a turning path still flows through its keys.
    RebuildHandles(keys, track == TRACK_ROLL);
}

void NormalizeAll(Doc& doc) {
    for (int32_t track = 0; track < TRACK_COUNT; track++) {
        Normalize(doc, track);
    }
}

int32_t FindKey(const Doc& doc, int32_t track, uint32_t time) {
    const std::vector<Key>& keys = doc.m_keys[track];

    for (size_t k = 0; k < keys.size(); k++) {
        if (keys[k].m_time == time) {
            return static_cast<int32_t>(k);
        }
    }

    return -1;
}

int32_t SetKey(Doc& doc, int32_t track, uint32_t time, const C3Vector& value) {
    int32_t index = FindKey(doc, track, time);

    if (index >= 0) {
        doc.m_keys[track][index].m_value = value;
    } else {
        Key key;
        key.m_time = time;
        key.m_value = value;
        doc.m_keys[track].push_back(key);
    }

    Normalize(doc, track);

    return FindKey(doc, track, time);
}

bool DeleteKey(Doc& doc, int32_t track, int32_t index) {
    std::vector<Key>& keys = doc.m_keys[track];

    if (index < 0 || index >= static_cast<int32_t>(keys.size())) {
        return false;
    }

    keys.erase(keys.begin() + index);
    Normalize(doc, track);

    return true;
}

int32_t SetKeyTime(Doc& doc, int32_t track, int32_t index, uint32_t time) {
    std::vector<Key>& keys = doc.m_keys[track];

    if (index < 0 || index >= static_cast<int32_t>(keys.size())) {
        return -1;
    }

    uint32_t low = index > 0 ? keys[index - 1].m_time + 1 : 0;
    uint32_t high = index + 1 < static_cast<int32_t>(keys.size()) ? keys[index + 1].m_time - 1 : UINT32_MAX;

    if (low > high) {
        return index;
    }

    keys[index].m_time = std::clamp(time, low, high);
    Normalize(doc, track);

    return index;
}

void SetHandle(Doc& doc, int32_t track, int32_t index, bool out, const C3Vector& offset) {
    std::vector<Key>& keys = doc.m_keys[track];

    if (index < 0 || index >= static_cast<int32_t>(keys.size())) {
        return;
    }

    size_t k = static_cast<size_t>(index);
    Key& key = keys[k];

    if (key.m_mode == TANGENT_AUTO || key.m_mode == TANGENT_STOP) {
        key.m_mode = TANGENT_SMOOTH;
    }

    (out ? key.m_out : key.m_in) = offset;
    C3Vector& other = out ? key.m_in : key.m_out;
    bool hasOther = out ? k > 0 : k + 1 < keys.size();
    bool scalar = track == TRACK_ROLL;

    if (hasOther && key.m_mode == TANGENT_SMOOTH) {
        other = MatchedHandle(keys, k, out, offset);
    } else if (hasOther && key.m_mode == TANGENT_ALIGNED) {
        float length = SignedLength(other, scalar);
        float own = SignedLength(offset, scalar);

        if (own > 0.0f) {
            other = Scale(offset, -length / own);
        }
    }

    // Smooth keys rebuild the in handle from the out handle, which now matches the one just placed.
    Normalize(doc, track);
}

void SetMode(Doc& doc, int32_t track, int32_t index, int32_t mode) {
    std::vector<Key>& keys = doc.m_keys[track];

    if (index < 0 || index >= static_cast<int32_t>(keys.size()) || mode < 0 || mode >= TANGENT_COUNT) {
        return;
    }

    Key& key = keys[index];
    key.m_mode = mode;

    // Aligned keys start out pointing the in handle opposite the out handle.
    if (mode == TANGENT_ALIGNED && index > 0 && index + 1 < static_cast<int32_t>(keys.size())) {
        float length = SignedLength(key.m_in, track == TRACK_ROLL);
        float own = SignedLength(key.m_out, track == TRACK_ROLL);

        if (own > 0.0f) {
            key.m_in = Scale(key.m_out, -length / own);
        }
    }

    Normalize(doc, track);
}

void SetModeAll(Doc& doc, int32_t track, int32_t mode) {
    if (mode < 0 || mode >= TANGENT_COUNT) {
        return;
    }

    for (Key& key : doc.m_keys[track]) {
        key.m_mode = mode;
    }

    Normalize(doc, track);
}

void SetInterp(Doc& doc, int32_t track, uint16_t interp) {
    if (interp > INTERP_HERMITE) {
        return;
    }

    doc.m_interp[track] = interp;
    Normalize(doc, track);
}

void SetLength(Doc& doc, uint32_t length, bool scaleKeys) {
    if (length < 1) {
        length = 1;
    }

    if (scaleKeys && doc.m_length > 0) {
        double ratio = static_cast<double>(length) / static_cast<double>(doc.m_length);

        for (std::vector<Key>& keys : doc.m_keys) {
            for (Key& key : keys) {
                key.m_time = static_cast<uint32_t>(std::llround(key.m_time * ratio));
            }
        }
    }

    doc.m_length = length;
    NormalizeAll(doc);
}

void SetOrigin(Doc& doc, const C3Vector& origin, float facing, bool keepWorld) {
    Doc before = doc;
    doc.m_origin = origin;
    doc.m_facing = facing;

    if (!keepWorld) {
        return;
    }

    for (int32_t track = TRACK_POSITION; track <= TRACK_TARGET; track++) {
        for (Key& key : doc.m_keys[track]) {
            key.m_value = ToLocal(doc, ToWorld(before, key.m_value));
            key.m_in = DirToLocal(doc, DirToWorld(before, key.m_in));
            key.m_out = DirToLocal(doc, DirToWorld(before, key.m_out));
        }
    }
}

static float SegmentLength(const std::vector<Key>& keys, uint16_t interp, size_t k) {
    float length = 0.0f;
    C3Vector previous = keys[k].m_value;

    for (int32_t step = 1; step <= kLengthSteps; step++) {
        C3Vector point = SegmentPoint(keys, interp, k, static_cast<float>(step) / kLengthSteps);
        length += Length(Subtract(point, previous));
        previous = point;
    }

    return length;
}

float PathLength(const Doc& doc, int32_t track) {
    const std::vector<Key>& keys = doc.m_keys[track];
    float length = 0.0f;

    for (size_t k = 0; k + 1 < keys.size(); k++) {
        length += SegmentLength(keys, doc.m_interp[track], k);
    }

    return length;
}

static uint32_t Remap(uint32_t time, const std::vector<uint32_t>& from, const std::vector<uint32_t>& to) {
    if (time <= from.front()) {
        return time;
    }

    if (time >= from.back()) {
        return time;
    }

    for (size_t k = 0; k + 1 < from.size(); k++) {
        if (time < from[k + 1]) {
            double ratio = static_cast<double>(time - from[k]) / static_cast<double>(from[k + 1] - from[k]);
            return static_cast<uint32_t>(std::llround(to[k] + ratio * (static_cast<double>(to[k + 1]) - to[k])));
        }
    }

    return time;
}

void ConstantSpeed(Doc& doc) {
    std::vector<Key>& keys = doc.m_keys[TRACK_POSITION];

    if (keys.size() < 3 || doc.m_interp[TRACK_POSITION] == INTERP_NONE) {
        return;
    }

    // Auto handles depend on the key times, a few passes settle them.
    for (int32_t pass = 0; pass < 4; pass++) {
        size_t count = keys.size();
        std::vector<float> distance(count, 0.0f);

        for (size_t k = 0; k + 1 < count; k++) {
            distance[k + 1] = distance[k] + SegmentLength(keys, doc.m_interp[TRACK_POSITION], k);
        }

        float total = distance.back();

        if (total <= 0.0f) {
            return;
        }

        std::vector<uint32_t> before(count);
        std::vector<uint32_t> after(count);
        uint32_t first = keys.front().m_time;
        uint32_t last = keys.back().m_time;

        for (size_t k = 0; k < count; k++) {
            before[k] = keys[k].m_time;
            after[k] = first + static_cast<uint32_t>(std::llround((last - first) * static_cast<double>(distance[k] / total)));

            if (k > 0 && after[k] <= after[k - 1]) {
                after[k] = after[k - 1] + 1;
            }
        }

        for (size_t k = 0; k < count; k++) {
            keys[k].m_time = after[k];
        }

        for (int32_t track = TRACK_TARGET; track < TRACK_COUNT; track++) {
            for (Key& key : doc.m_keys[track]) {
                key.m_time = Remap(key.m_time, before, after);
            }
        }

        NormalizeAll(doc);
    }
}

static constexpr float kPi = 3.14159265f;

// Time weighted Gaussian average, uneven key spacing is fine.
static void GaussianSmooth(std::vector<C3Vector>& values, const std::vector<uint32_t>& times, float sigma, bool keepEnds) {
    size_t count = values.size();

    if (sigma <= 0.0f || count < 3) {
        return;
    }

    std::vector<C3Vector> result = values;

    for (size_t i = 0; i < count; i++) {
        if (keepEnds && (i == 0 || i == count - 1)) {
            continue;
        }

        C3Vector sum{};
        float weights = 0.0f;

        for (size_t j = 0; j < count; j++) {
            float dt = (static_cast<float>(times[j]) - static_cast<float>(times[i])) / sigma;

            if (fabsf(dt) > 3.0f) {
                continue;
            }

            float weight = expf(-0.5f * dt * dt);
            sum = Add(sum, Scale(values[j], weight));
            weights += weight;
        }

        result[i] = Scale(sum, 1.0f / weights);
    }

    values = result;
}

static C3Vector LookDirection(const C3Vector& look) {
    return { cosf(look.y) * cosf(look.x), cosf(look.y) * sinf(look.x), sinf(look.y) };
}

float RollForBank(const C3Vector& forward, float bank, float previous) {
    C3Vector f = VectorMath::Normalize(forward);
    C3Vector z{ 0.0f, 0.0f, 1.0f };
    C3Vector left = Cross(z, f);

    if (Length(left) < 1e-4f) {
        return previous;
    }

    left = VectorMath::Normalize(left);
    C3Vector level = VectorMath::Normalize(Subtract(z, Scale(f, Dot(z, f))));
    C3Vector up = Add(Scale(level, cosf(bank)), Scale(left, sinf(bank)));

    // The client's up hint is (0, sin r, cos r), it has to lie in the plane of the view and the wanted up.
    C3Vector normal = Cross(f, up);

    if (normal.y * normal.y + normal.z * normal.z < 1e-6f) {
        return previous;
    }

    float roll = atan2f(-normal.z, normal.y);

    if (sinf(roll) * up.y + cosf(roll) * up.z < 0.0f) {
        roll += kPi;
    }

    while (roll - previous > kPi) {
        roll -= 2.0f * kPi;
    }

    while (roll - previous < -kPi) {
        roll += 2.0f * kPi;
    }

    // Near 90 degrees the hint lines up with the view and the client falls back to a fixed axis.
    return std::clamp(roll, -1.3f, 1.3f);
}

void SmoothCamera(Doc& doc, const SmoothOptions& options) {
    std::vector<Key>& positions = doc.m_keys[TRACK_POSITION];
    size_t count = positions.size();

    if (count < 2) {
        return;
    }

    std::vector<uint32_t> times(count);

    for (size_t k = 0; k < count; k++) {
        times[k] = positions[k].m_time;
    }

    // Look at each key as yaw, pitch and distance, read before the path moves.
    std::vector<C3Vector> look(count);
    bool hasTarget = !doc.m_keys[TRACK_TARGET].empty();

    for (size_t k = 0; k < count; k++) {
        C3Vector position = positions[k].m_value;
        C3Vector target = hasTarget ? Evaluate(doc, TRACK_TARGET, static_cast<float>(times[k])) : Add(position, C3Vector{ 20.0f, 0.0f, 0.0f });
        C3Vector offset = Subtract(target, position);
        float distance = Length(offset);

        if (distance < 0.01f) {
            look[k] = k > 0 ? look[k - 1] : C3Vector{ 0.0f, 0.0f, 20.0f };
            continue;
        }

        float yaw = atan2f(offset.y, offset.x);

        if (k > 0) {
            while (yaw - look[k - 1].x > kPi) {
                yaw -= 2.0f * kPi;
            }

            while (yaw - look[k - 1].x < -kPi) {
                yaw += 2.0f * kPi;
            }
        }

        look[k] = { yaw, asinf(std::clamp(offset.z / distance, -1.0f, 1.0f)), distance };
    }

    if (options.m_pathSeconds > 0.0f) {
        std::vector<C3Vector> values(count);

        for (size_t k = 0; k < count; k++) {
            values[k] = positions[k].m_value;
        }

        GaussianSmooth(values, times, options.m_pathSeconds * 1000.0f, true);

        for (size_t k = 0; k < count; k++) {
            positions[k].m_value = values[k];
            positions[k].m_mode = TANGENT_AUTO;
        }

        Normalize(doc, TRACK_POSITION);
    }

    GaussianSmooth(look, times, options.m_lookSeconds * 1000.0f, true);

    // One distance for every key, so only the direction changes along the path.
    std::vector<float> distances(count);

    for (size_t k = 0; k < count; k++) {
        distances[k] = look[k].z;
    }

    std::nth_element(distances.begin(), distances.begin() + count / 2, distances.end());
    float distance = std::max(2.0f, distances[count / 2]);
    std::vector<C3Vector> offsets(count);

    for (size_t k = 0; k < count; k++) {
        offsets[k] = Scale(LookDirection(look[k]), distance);
    }

    // Target minus position is its own cubic on every segment, its handles stop where the look turns back.
    std::vector<Key> targets(count);

    for (size_t k = 0; k < count; k++) {
        C3Vector velocity = AutoVelocity(offsets, times, k, true);
        Key& key = targets[k];
        key.m_time = times[k];
        key.m_value = Add(positions[k].m_value, offsets[k]);
        key.m_in = k > 0 ? Add(positions[k].m_in, Scale(velocity, -SegmentTime(positions, k - 1, k) / 3.0f)) : C3Vector{};
        key.m_out = k + 1 < count ? Add(positions[k].m_out, Scale(velocity, SegmentTime(positions, k, k + 1) / 3.0f)) : C3Vector{};
        key.m_mode = TANGENT_FREE;
    }

    doc.m_keys[TRACK_TARGET] = targets;
    doc.m_interp[TRACK_TARGET] = doc.m_interp[TRACK_POSITION];

    if (!options.m_bank) {
        return;
    }

    // Bank like a plane, the lean grows with speed times turn rate.
    std::vector<C3Vector> banks(count);

    for (size_t k = 0; k < count; k++) {
        size_t from = k > 0 ? k - 1 : k;
        size_t to = k + 1 < count ? k + 1 : k;
        float span = static_cast<float>(times[to]) - static_cast<float>(times[from]);
        float turnRate = span > 0.0f ? (look[to].x - look[from].x) / span * 1000.0f : 0.0f;
        float t = static_cast<float>(times[k]);
        float t0 = std::max(t - 10.0f, static_cast<float>(times.front()));
        float t1 = std::min(t + 10.0f, static_cast<float>(times.back()));
        float speed = t1 > t0 ? Length(Subtract(Evaluate(doc, TRACK_POSITION, t1), Evaluate(doc, TRACK_POSITION, t0))) / (t1 - t0) * 1000.0f : 0.0f;
        float bank = options.m_bankStrength * atanf(speed * turnRate / 9.8f);
        banks[k] = { std::clamp(bank, -options.m_maxBank, options.m_maxBank), 0.0f, 0.0f };
    }

    GaussianSmooth(banks, times, options.m_lookSeconds * 1000.0f, false);

    std::vector<Key> rolls(count);
    float previous = 0.0f;

    for (size_t k = 0; k < count; k++) {
        previous = RollForBank(DirToWorld(doc, LookDirection(look[k])), banks[k].x, previous);
        rolls[k].m_time = times[k];
        rolls[k].m_value = { previous, 0.0f, 0.0f };
        rolls[k].m_mode = TANGENT_AUTO;
    }

    doc.m_keys[TRACK_ROLL] = rolls;
    doc.m_interp[TRACK_ROLL] = IsSpline(doc.m_interp[TRACK_POSITION]) ? doc.m_interp[TRACK_POSITION] : INTERP_LINEAR;
    Normalize(doc, TRACK_ROLL);
}

void ShiftTimes(Doc& doc, int32_t delta) {
    for (std::vector<Key>& keys : doc.m_keys) {
        for (Key& key : keys) {
            int64_t time = static_cast<int64_t>(key.m_time) + delta;
            key.m_time = time < 0 ? 0 : static_cast<uint32_t>(time);
        }
    }

    NormalizeAll(doc);
}

void SpeedSamples(const Doc& doc, int32_t track, uint32_t count, std::vector<float>& speeds) {
    speeds.assign(count, 0.0f);

    if (count == 0 || doc.m_length == 0) {
        return;
    }

    constexpr int32_t kSub = 4;
    float step = static_cast<float>(doc.m_length) / static_cast<float>(count);

    for (uint32_t i = 0; i < count; i++) {
        float start = step * i;
        C3Vector previous = Evaluate(doc, track, start);
        float length = 0.0f;

        for (int32_t sub = 1; sub <= kSub; sub++) {
            C3Vector point = Evaluate(doc, track, start + step * sub / kSub);
            length += track == TRACK_ROLL ? fabsf(point.x - previous.x) : Length(Subtract(point, previous));
            previous = point;
        }

        speeds[i] = length / step * 1000.0f;
    }
}

// Writing.

class Writer {
    public:
    std::vector<uint8_t>& m_data;

    explicit Writer(std::vector<uint8_t>& data) : m_data(data) {}

    template <class T>
    void Put(uint32_t offset, T value) {
        if (m_data.size() < offset + sizeof(T)) {
            m_data.resize(offset + sizeof(T), 0);
        }

        memcpy(m_data.data() + offset, &value, sizeof(T));
    }

    void PutVector(uint32_t offset, const C3Vector& value) {
        this->Put(offset, value.x);
        this->Put(offset + 4, value.y);
        this->Put(offset + 8, value.z);
    }

    void PutArray(uint32_t offset, uint32_t count, uint32_t target) {
        this->Put(offset, count);
        this->Put(offset + 4, count ? target : 0u);
    }

    void PutEmptyBounds(uint32_t offset) {
        for (uint32_t i = 0; i < 3; i++) {
            this->Put(offset + i * 4, FLT_MAX);
            this->Put(offset + 12 + i * 4, -FLT_MAX);
        }
    }

    void PutEmptyTrack(uint32_t offset) {
        this->Put<uint16_t>(offset, 0);
        this->Put<uint16_t>(offset + 2, 0xFFFF);
    }
};

static uint32_t Align(uint32_t value) {
    return (value + 15u) & ~15u;
}

// Keys as the client will read them: one written twice so it animates, a hold from 0 when the first key starts later.
static std::vector<Key> OutputKeys(const std::vector<Key>& keys, uint32_t length) {
    std::vector<Key> output = keys;

    if (output.empty()) {
        return output;
    }

    if (output.front().m_time > 0) {
        Key hold = output.front();
        hold.m_time = 0;
        hold.m_in = {};
        hold.m_out = {};
        output.front().m_in = {};
        output.insert(output.begin(), hold);
    }

    if (output.size() == 1) {
        Key copy = output.front();
        copy.m_time = std::max<uint32_t>(length, copy.m_time + 1);
        output.push_back(copy);
    }

    return output;
}

void Build(const Doc& doc, const std::string& name, std::vector<uint8_t>& m2, std::vector<uint8_t>& skin) {
    m2.clear();
    Writer out(m2);

    uint32_t nameLength = static_cast<uint32_t>(name.size()) + 1;
    uint32_t sequences = Align(kHeaderSize + nameLength);
    uint32_t bones = sequences + kSequenceSize;
    uint32_t keyBoneLookup = Align(bones + kBoneSize);
    uint32_t cameraLookup = keyBoneLookup + 16;
    uint32_t camera = cameraLookup + 16;
    uint32_t arrays = Align(camera + kCameraSize);
    uint32_t cursor = Align(arrays + TRACK_COUNT * 16);

    out.Put(0x000, kMagic);
    out.Put(0x004, kVersion);
    out.PutArray(0x008, nameLength, kHeaderSize);
    out.PutArray(0x01C, 1, sequences);
    out.PutArray(0x02C, 1, bones);
    out.PutArray(0x034, 1, keyBoneLookup);
    out.Put<uint32_t>(0x044, 1);
    out.PutEmptyBounds(0x0A0);
    out.PutArray(0x110, 1, camera);
    out.PutArray(0x118, 1, cameraLookup);

    for (uint32_t i = 0; i < nameLength - 1; i++) {
        out.Put<char>(kHeaderSize + i, name[i]);
    }

    out.Put<char>(kHeaderSize + nameLength - 1, 0);

    out.Put<uint32_t>(sequences + 0x04, doc.m_length);
    out.Put<uint32_t>(sequences + 0x0C, 0x20);
    out.Put<int16_t>(sequences + 0x10, 0x7FFF);
    out.PutEmptyBounds(sequences + 0x20);
    out.Put<int16_t>(sequences + 0x3C, -1);

    out.Put<int32_t>(bones + 0x00, -1);
    out.Put<int16_t>(bones + 0x08, -1);
    out.PutEmptyTrack(bones + 0x10);
    out.PutEmptyTrack(bones + 0x24);
    out.PutEmptyTrack(bones + 0x38);
    out.Put<float>(bones + 0x54, 0.0f);

    out.Put<uint16_t>(keyBoneLookup, 0xFFFF);
    out.Put<uint16_t>(cameraLookup, 0xFFFF);

    out.Put<int32_t>(camera + 0x00, -1);
    out.Put(camera + 0x04, doc.m_fieldOfView);
    out.Put(camera + 0x08, doc.m_farClip);
    out.Put(camera + 0x0C, doc.m_nearClip);

    static const uint32_t s_trackOffsets[TRACK_COUNT] = { 0x10, 0x30, 0x50 };

    for (int32_t track = 0; track < TRACK_COUNT; track++) {
        std::vector<Key> keys = OutputKeys(doc.m_keys[track], doc.m_length);
        uint16_t interp = doc.m_interp[track];
        uint32_t header = camera + s_trackOffsets[track];
        uint32_t timesHeader = arrays + track * 16;
        uint32_t keysHeader = timesHeader + 8;
        uint32_t count = static_cast<uint32_t>(keys.size());
        bool scalar = track == TRACK_ROLL;
        uint32_t keySize = scalar ? 12 : 36;

        out.Put<uint16_t>(header, interp);
        out.Put<uint16_t>(header + 2, 0xFFFF);
        out.PutArray(header + 4, 1, timesHeader);
        out.PutArray(header + 12, 1, keysHeader);

        uint32_t timesData = cursor;
        uint32_t keysData = Align(timesData + count * 4);
        cursor = Align(keysData + count * keySize);
        out.PutArray(timesHeader, count, timesData);
        out.PutArray(keysHeader, count, keysData);

        for (uint32_t k = 0; k < count; k++) {
            const Key& key = keys[k];
            C3Vector in;
            C3Vector outTan;

            if (interp == INTERP_HERMITE) {
                in = Scale(key.m_in, -3.0f);
                outTan = Scale(key.m_out, 3.0f);
            } else {
                in = Add(key.m_value, key.m_in);
                outTan = Add(key.m_value, key.m_out);
            }

            out.Put(timesData + k * 4, key.m_time);
            uint32_t at = keysData + k * keySize;

            if (scalar) {
                out.Put(at, key.m_value.x);
                out.Put(at + 4, in.x);
                out.Put(at + 8, outTan.x);
            } else {
                out.PutVector(at, key.m_value);
                out.PutVector(at + 12, in);
                out.PutVector(at + 24, outTan);
            }
        }
    }

    // A blank skin profile, the client wants one even with no geometry.
    m2.resize(cursor, 0);
    skin.assign(0x30, 0);
    memcpy(skin.data(), "SKIN", 4);
    uint32_t boneCountMax = 21;
    memcpy(skin.data() + 0x2C, &boneCountMax, 4);
}

// Reading.

class Reader {
    public:
    const uint8_t* m_data;
    size_t m_size;

    bool Has(uint64_t offset, uint64_t size) const {
        return offset + size <= m_size;
    }

    template <class T>
    bool Get(uint64_t offset, T& value) const {
        if (!this->Has(offset, sizeof(T))) {
            return false;
        }

        memcpy(&value, m_data + offset, sizeof(T));
        return true;
    }

    bool GetVector(uint64_t offset, C3Vector& value) const {
        return this->Get(offset, value.x) && this->Get(offset + 4, value.y) && this->Get(offset + 8, value.z);
    }
};

static bool ParseTrack(const Reader& in, uint32_t header, bool scalar, const C3Vector& base, Doc& doc, int32_t track, std::string& error) {
    static const char* const s_names[TRACK_COUNT] = { "position", "target", "roll" };
    uint16_t interp = 0;
    uint32_t timeArrays = 0;
    uint32_t timeArraysOffset = 0;
    uint32_t keyArrays = 0;
    uint32_t keyArraysOffset = 0;

    if (!in.Get(header, interp) || !in.Get(header + 4, timeArrays) || !in.Get(header + 8, timeArraysOffset) || !in.Get(header + 12, keyArrays) || !in.Get(header + 16, keyArraysOffset)) {
        error = std::string("the ") + s_names[track] + " track is cut off";
        return false;
    }

    doc.m_interp[track] = interp <= INTERP_HERMITE ? interp : INTERP_LINEAR;
    doc.m_keys[track].clear();

    if (timeArrays == 0 || keyArrays == 0) {
        return true;
    }

    uint32_t timeCount = 0;
    uint32_t timeOffset = 0;
    uint32_t keyCount = 0;
    uint32_t keyOffset = 0;
    uint32_t keySize = scalar ? 12 : 36;

    if (!in.Get(timeArraysOffset, timeCount) || !in.Get(timeArraysOffset + 4, timeOffset) || !in.Get(keyArraysOffset, keyCount) || !in.Get(keyArraysOffset + 4, keyOffset)) {
        error = std::string("the ") + s_names[track] + " track arrays are cut off";
        return false;
    }

    uint32_t count = std::min(timeCount, keyCount);

    if (!in.Has(timeOffset, static_cast<uint64_t>(count) * 4) || !in.Has(keyOffset, static_cast<uint64_t>(count) * keySize)) {
        error = std::string("the ") + s_names[track] + " keys are cut off";
        return false;
    }

    bool spline = IsSpline(doc.m_interp[track]);

    for (uint32_t k = 0; k < count; k++) {
        Key key;
        C3Vector in3;
        C3Vector out3;
        uint32_t at = keyOffset + k * keySize;
        in.Get(timeOffset + k * 4, key.m_time);

        if (scalar) {
            in.Get(at, key.m_value.x);
            in.Get(at + 4, in3.x);
            in.Get(at + 8, out3.x);
        } else {
            in.GetVector(at, key.m_value);
            in.GetVector(at + 12, in3);
            in.GetVector(at + 24, out3);
        }

        if (doc.m_interp[track] == INTERP_HERMITE) {
            key.m_in = Scale(in3, -1.0f / 3.0f);
            key.m_out = Scale(out3, 1.0f / 3.0f);
        } else if (spline) {
            key.m_in = Subtract(in3, key.m_value);
            key.m_out = Subtract(out3, key.m_value);
        }

        key.m_value = Add(key.m_value, base);
        // Loaded handles are kept exactly, linear keys have none so they can go auto.
        key.m_mode = spline ? TANGENT_FREE : TANGENT_AUTO;
        doc.m_keys[track].push_back(key);
    }

    return true;
}

bool Parse(const uint8_t* data, size_t size, Doc& doc, std::string& error) {
    Reader in{ data, size };
    uint32_t magic = 0;
    uint32_t version = 0;

    if (!in.Get(0, magic) || magic != kMagic) {
        error = "not an MD20 model";
        return false;
    }

    in.Get(4, version);

    if (version < 0x104 || version > 0x108) {
        error = "model version " + std::to_string(version) + " is not a 3.x model";
        return false;
    }

    uint32_t cameraCount = 0;
    uint32_t camera = 0;

    if (!in.Get(0x110, cameraCount) || !in.Get(0x114, camera) || cameraCount == 0 || !in.Has(camera, kCameraSize)) {
        error = "the model has no camera";
        return false;
    }

    Doc result;
    uint32_t sequenceCount = 0;
    uint32_t sequences = 0;
    in.Get(0x1C, sequenceCount);
    in.Get(0x20, sequences);

    if (sequenceCount > 0) {
        in.Get(sequences + 4, result.m_length);
    }

    in.Get(camera + 0x04, result.m_fieldOfView);
    in.Get(camera + 0x08, result.m_farClip);
    in.Get(camera + 0x0C, result.m_nearClip);

    C3Vector positionBase;
    C3Vector targetBase;
    in.GetVector(camera + 0x24, positionBase);
    in.GetVector(camera + 0x44, targetBase);

    if (!ParseTrack(in, camera + 0x10, false, positionBase, result, TRACK_POSITION, error) || !ParseTrack(in, camera + 0x30, false, targetBase, result, TRACK_TARGET, error) || !ParseTrack(in, camera + 0x50, true, C3Vector{}, result, TRACK_ROLL, error)) {
        return false;
    }

    if (result.m_length == 0) {
        for (const std::vector<Key>& keys : result.m_keys) {
            if (!keys.empty()) {
                result.m_length = std::max(result.m_length, keys.back().m_time);
            }
        }
    }

    // Keep exactly what was read, Normalize would only reorder broken files.
    for (int32_t track = 0; track < TRACK_COUNT; track++) {
        std::vector<Key>& keys = result.m_keys[track];
        std::stable_sort(keys.begin(), keys.end(), KeyTimeLess);
    }

    result.m_path = doc.m_path;
    result.m_origin = doc.m_origin;
    result.m_facing = doc.m_facing;
    doc = result;

    return true;
}

std::string Validate(const Doc& doc) {
    std::string problems;
    static const char* const s_names[TRACK_COUNT] = { "Position", "Target", "Roll" };

    for (int32_t track = TRACK_POSITION; track <= TRACK_TARGET; track++) {
        if (doc.m_keys[track].empty()) {
            problems += std::string(s_names[track]) + " has no keys, the camera would sit at the origin.\n";
        }
    }

    for (int32_t track = 0; track < TRACK_COUNT; track++) {
        const std::vector<Key>& keys = doc.m_keys[track];

        if (!keys.empty() && keys.back().m_time > doc.m_length) {
            problems += std::string(s_names[track]) + " has keys after the end, they never play.\n";
        }
    }

    if (doc.m_length < 100) {
        problems += "The length is under 0.1 seconds.\n";
    }

    return problems;
}

}
