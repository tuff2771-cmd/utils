#pragma once
#include <Geode/Geode.hpp>
#include <type_traits>

// All cheat settings live here. Saved/loaded automatically.
struct Config {
    // ---- Gameplay ----
    bool clickGreenDash = false;
    bool gdP1 = true, gdP2 = true;

    bool noOrbEffects = false;
    bool clickBlackOrb = false;
    bool easyClickBlackUfo = false;
    bool clickJumpPads = false;
    bool clickGravityPads = false;

    bool maintainGravity = false;
    bool mgP1 = true, mgP2 = true;

    bool autoStraightFly = false;
    bool asfP1 = true, asfP2 = true;
    float asfThreshold = 5.f;

    bool autoStraightUfo = false;
    bool asuP1 = true, asuP2 = true;
    float asuThreshold = 10.f;
    float asuTargetY = 0.f; // 0 = use the height where the ufo started

    bool ac1 = false;
    float ac1Every = 10.f;
    float ac1Hold = 2.f;
    bool ac1Swift = false;

    bool ac2 = false;
    float ac2Every = 10.f;
    float ac2Hold = 2.f;
    bool ac2Swift = false;

    bool noclip = false;
    bool ncP1 = true, ncP2 = true;

    bool autoSwift = false;

    bool link2P = false;
    int linkMode = 0;   // 0 = invert, 1 = copy
    int linkDriver = 0; // 0 = player 1 drives, 1 = player 2 drives

    bool waveGap = false;
    bool wgP1 = true;
    float wgThr1 = 3.f;
    bool wgOrb1 = false;
    bool wgP2 = true;
    float wgThr2 = 3.f;
    bool wgOrb2 = false;

    template <class F>
    void each(F&& f) {
        f("clickGreenDash", clickGreenDash); f("gdP1", gdP1); f("gdP2", gdP2);
        f("noOrbEffects", noOrbEffects);
        f("clickBlackOrb", clickBlackOrb);
        f("easyClickBlackUfo", easyClickBlackUfo);
        f("clickJumpPads", clickJumpPads);
        f("clickGravityPads", clickGravityPads);
        f("maintainGravity", maintainGravity); f("mgP1", mgP1); f("mgP2", mgP2);
        f("autoStraightFly", autoStraightFly); f("asfP1", asfP1); f("asfP2", asfP2); f("asfThreshold", asfThreshold);
        f("autoStraightUfo", autoStraightUfo); f("asuP1", asuP1); f("asuP2", asuP2);
        f("asuThreshold", asuThreshold); f("asuTargetY", asuTargetY);
        f("ac1", ac1); f("ac1Every", ac1Every); f("ac1Hold", ac1Hold); f("ac1Swift", ac1Swift);
        f("ac2", ac2); f("ac2Every", ac2Every); f("ac2Hold", ac2Hold); f("ac2Swift", ac2Swift);
        f("noclip", noclip); f("ncP1", ncP1); f("ncP2", ncP2);
        f("autoSwift", autoSwift);
        f("link2P", link2P); f("linkMode", linkMode); f("linkDriver", linkDriver);
        f("waveGap", waveGap);
        f("wgP1", wgP1); f("wgThr1", wgThr1); f("wgOrb1", wgOrb1);
        f("wgP2", wgP2); f("wgThr2", wgThr2); f("wgOrb2", wgOrb2);
    }

    void load() {
        each([](char const* k, auto& v) {
            using T = std::decay_t<decltype(v)>;
            v = geode::Mod::get()->getSavedValue<T>(k, v);
        });
    }

    void save() {
        each([](char const* k, auto& v) {
            using T = std::decay_t<decltype(v)>;
            geode::Mod::get()->setSavedValue<T>(k, v);
        });
    }
};

inline Config& cfg() {
    static Config c;
    return c;
}
