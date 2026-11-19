#ifndef HOT_EDITOR_CAMERA_CAMERADOC_H
#define HOT_EDITOR_CAMERA_CAMERADOC_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Tempest/C3Vector.h"

// A cinematic camera M2: one sequence, one camera with position, target and roll spline tracks.
namespace HoT::Editor::Camera {

enum TRACK : int32_t {
    TRACK_POSITION = 0,
    TRACK_TARGET = 1,
    TRACK_ROLL = 2,
    TRACK_COUNT = 3,
};

// M2TRACKTYPE values, one per track.
enum INTERP : uint16_t {
    INTERP_NONE = 0,
    INTERP_LINEAR = 1,
    INTERP_BEZIER = 2,
    INTERP_HERMITE = 3,
};

enum TANGENT : int32_t {
    // Handles follow the neighbours so the speed is continuous through the key.
    TANGENT_AUTO = 0,
    // Hand placed, the other handle mirrors it scaled by the segment lengths so the speed stays continuous.
    TANGENT_SMOOTH = 1,
    // Hand placed, the other handle keeps its length and points the opposite way.
    TANGENT_ALIGNED = 2,
    TANGENT_FREE = 3,
    // No handles, the camera eases to a stop on the key.
    TANGENT_STOP = 4,
    TANGENT_COUNT = 5,
};

// Handles are Bezier style offsets from the value, Hermite tracks convert on load and save so switching keeps the shape.
// Roll uses x only.
struct Key {
    uint32_t m_time = 0;
    C3Vector m_value{};
    C3Vector m_in{};
    C3Vector m_out{};
    int32_t m_mode = TANGENT_AUTO;
};

struct Doc {
    std::string m_path;
    uint32_t m_length = 10000;
    // CinematicCamera.dbc origin and facing, keys are local to them.
    C3Vector m_origin{};
    float m_facing = 0.0f;
    uint16_t m_interp[TRACK_COUNT] = { INTERP_BEZIER, INTERP_BEZIER, INTERP_BEZIER };
    std::vector<Key> m_keys[TRACK_COUNT];
    float m_fieldOfView = 0.785398f;
    float m_farClip = 27.7778f;
    float m_nearClip = 0.222222f;
};

bool IsSpline(uint16_t interp);

C3Vector ToWorld(const Doc& doc, const C3Vector& local);
C3Vector ToLocal(const Doc& doc, const C3Vector& world);
C3Vector DirToWorld(const Doc& doc, const C3Vector& local);
C3Vector DirToLocal(const Doc& doc, const C3Vector& world);

// Same result as M2AnimateSplineTrack at that time, in local space, with fractional milliseconds.
C3Vector Evaluate(const Doc& doc, int32_t track, float time);
C3Vector EvaluateWorld(const Doc& doc, int32_t track, float time);

// Keys sorted by time with no two on the same millisecond, auto handles rebuilt and smooth handles matched.
void Normalize(Doc& doc, int32_t track);
void NormalizeAll(Doc& doc);

// Index of the key on that millisecond or -1.
int32_t FindKey(const Doc& doc, int32_t track, uint32_t time);
// Adds a key or replaces the value of the one on that millisecond, returns its index after sorting.
int32_t SetKey(Doc& doc, int32_t track, uint32_t time, const C3Vector& value);
bool DeleteKey(Doc& doc, int32_t track, int32_t index);
// Moves a key in time, it may not pass its neighbours. Returns the index after the move.
int32_t SetKeyTime(Doc& doc, int32_t track, int32_t index, uint32_t time);

// Places one handle and applies the key's tangent mode to the other one, auto and stop keys become smooth.
void SetHandle(Doc& doc, int32_t track, int32_t index, bool out, const C3Vector& offset);
void SetMode(Doc& doc, int32_t track, int32_t index, int32_t mode);
void SetModeAll(Doc& doc, int32_t track, int32_t mode);

// Changes the interpolation, the handles keep their shape between Bezier and Hermite.
void SetInterp(Doc& doc, int32_t track, uint16_t interp);

// Changes the sequence length, optionally scaling every key time with it.
void SetLength(Doc& doc, uint32_t length, bool scaleKeys);

// Moves the origin and facing, keepWorld leaves the path where it is in the world.
void SetOrigin(Doc& doc, const C3Vector& origin, float facing, bool keepWorld);

// Spaces the position keys by path length so the camera moves at an even speed, target and roll keys follow the remap.
void ConstantSpeed(Doc& doc);

struct SmoothOptions {
    // Seconds of look and path to average over, 0 leaves that part alone.
    float m_lookSeconds = 1.0f;
    float m_pathSeconds = 0.0f;
    bool m_bank = false;
    float m_bankStrength = 0.5f;
    // Radians.
    float m_maxBank = 0.35f;
};

// Rebuilds the target track on the position keys from a smoothed look direction, so the view cannot swing between keys.
// Optionally smooths the path first and rebuilds the roll track to bank into turns.
void SmoothCamera(Doc& doc, const SmoothOptions& options);

// The client roll value that gives bank radians at a world view direction, nearest to previous.
// The client tilts the up vector around world X, so banking fades out as the view turns towards +-Y.
float RollForBank(const C3Vector& forward, float bank, float previous);

// Moves every key of every track by delta milliseconds, keys stay at 0 or later.
void ShiftTimes(Doc& doc, int32_t delta);

// Path length of the position track in yards.
float PathLength(const Doc& doc, int32_t track);

// Speed of the track in yards a second at evenly spaced times over the length.
void SpeedSamples(const Doc& doc, int32_t track, uint32_t count, std::vector<float>& speeds);

// Writes the M2 and its 00.skin, single keys are written twice so the client animates them.
void Build(const Doc& doc, const std::string& name, std::vector<uint8_t>& m2, std::vector<uint8_t>& skin);

// Reads camera 0 of any M2, the position and target base are folded into the keys.
bool Parse(const uint8_t* data, size_t size, Doc& doc, std::string& error);

// Problems that would make the client play the camera wrong, empty when there are none.
std::string Validate(const Doc& doc);

}

#endif
