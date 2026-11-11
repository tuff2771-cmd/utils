#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/RingObject.hpp>
#include <Geode/modify/GameObject.hpp>

#include "Config.hpp"
#include "Menu.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <initializer_list>
#include <vector>

using namespace geode::prelude;

$on_mod(Loaded) {
    cfg().load();
}

namespace {

// ------------------------------------------------------------------
// Object cache: orbs/pads and solids, sorted by X so scans stay cheap
// ------------------------------------------------------------------
struct Cache {
    GJBaseGameLayer* owner = nullptr;
    std::vector<GameObject*> interactive;
    std::vector<GameObject*> solids;

    void clear() {
        owner = nullptr;
        interactive.clear();
        solids.clear();
    }

    void build(GJBaseGameLayer* l) {
        clear();
        owner = l;
        if (!l->m_objects) return;
        for (auto obj : CCArrayExt<GameObject*>(l->m_objects)) {
            switch (obj->m_objectType) {
                case GameObjectType::DashRing:
                case GameObjectType::DropRing:
                case GameObjectType::GravityRing:
                case GameObjectType::YellowJumpPad:
                case GameObjectType::PinkJumpPad:
                case GameObjectType::RedJumpPad:
                case GameObjectType::GravityPad:
                    interactive.push_back(obj);
                    break;
                case GameObjectType::Solid:
                    solids.push_back(obj);
                    break;
                default:
                    break;
            }
        }
        auto byX = [](GameObject* a, GameObject* b) { return a->getPositionX() < b->getPositionX(); };
        std::sort(interactive.begin(), interactive.end(), byX);
        std::sort(solids.begin(), solids.end(), byX);
    }
};

// Per-attempt runtime state (index 0 = player 1, 1 = player 2)
struct Runtime {
    int frame = 0;
    bool held[2] = {false, false};
    bool lockInit[2] = {false, false};
    bool lockFlip[2] = {false, false};
    bool wasShip[2] = {false, false};
    bool wasUfo[2] = {false, false};
    float shipY[2] = {0.f, 0.f};
    float ufoY[2] = {0.f, 0.f};
};

Cache g_cache;
Runtime g_rt;
bool g_inLink = false;

template <class F>
void nearby(std::vector<GameObject*> const& v, float x, float range, F&& f) {
    auto it = std::lower_bound(v.begin(), v.end(), x - range,
                               [](GameObject* o, float val) { return o->getPositionX() < val; });
    for (; it != v.end() && (*it)->getPositionX() <= x + range; ++it) f(*it);
}

// How many of these orb/pad types the player is touching right now
int touching(PlayerObject* p, std::initializer_list<GameObjectType> types) {
    int n = 0;
    auto pr = p->getObjectRect();
    nearby(g_cache.interactive, p->getPositionX(), 80.f, [&](GameObject* o) {
        for (auto t : types) {
            if (o->m_objectType == t) {
                if (pr.intersectsRect(o->getObjectRect())) n++;
                break;
            }
        }
    });
    return n;
}

// "Swift click": quick press + release, repeated n times in one tick (more orbs = more clicks)
void swiftClick(GJBaseGameLayer* l, bool isP1, int n) {
    n = std::clamp(n, 1, 8);
    for (int k = 0; k < n; k++) {
        l->handleButton(true, 1, isP1);
        l->handleButton(false, 1, isP1);
    }
    g_rt.held[isP1 ? 0 : 1] = false;
}

void setHeld(GJBaseGameLayer* l, bool isP1, bool down) {
    int i = isP1 ? 0 : 1;
    if (g_rt.held[i] == down) return;
    g_rt.held[i] = down;
    l->handleButton(down, 1, isP1);
}

void tick(GJBaseGameLayer* l) {
    auto& c = cfg();
    if (g_cache.owner != l) g_cache.build(l);
    g_rt.frame++;

    bool dual = l->m_gameState.m_isDualMode;

    struct Want { bool active = false; bool down = false; } want[2];

    for (int i = 0; i < 2; i++) {
        bool isP1 = (i == 0);
        PlayerObject* p = isP1 ? l->m_player1 : l->m_player2;
        if (!p || (!isP1 && !dual) || p->m_isDead) {
            if (g_rt.held[i] && l->m_player1) setHeld(l, isP1, false);
            continue;
        }

        float py = p->getPositionY();
        float dir = p->m_isUpsideDown ? -1.f : 1.f; // flipped gravity flips what "hold" does
        int swift = 0;

        // ---- Maintain gravity ----
        if (c.maintainGravity && (isP1 ? c.mgP1 : c.mgP2)) {
            if (!g_rt.lockInit[i]) {
                g_rt.lockInit[i] = true;
                g_rt.lockFlip[i] = p->m_isUpsideDown;
            } else if (p->m_isUpsideDown != g_rt.lockFlip[i]) {
                p->flipGravity(g_rt.lockFlip[i], true);
            }
        } else {
            g_rt.lockInit[i] = false;
        }

        // ---- Orbs / pads ----
        if (c.clickGreenDash && (isP1 ? c.gdP1 : c.gdP2)) {
            swift += touching(p, {GameObjectType::DashRing});
        }
        if (c.clickBlackOrb) {
            swift += touching(p, {GameObjectType::DropRing});
        }
        if (c.easyClickBlackUfo && p->m_isBird) {
            // black orb pushes toward the floor: skip if already moving toward it
            bool towardFloor = p->m_isUpsideDown ? (p->m_yVelocity > 0) : (p->m_yVelocity < 0);
            if (!towardFloor) swift += touching(p, {GameObjectType::DropRing});
        }
        if (c.clickJumpPads) {
            swift += touching(p, {GameObjectType::YellowJumpPad, GameObjectType::PinkJumpPad, GameObjectType::RedJumpPad});
        }
        if (c.clickGravityPads) {
            swift += touching(p, {GameObjectType::GravityPad});
        }

        // ---- Auto straight fly (ship) ----
        if (c.autoStraightFly && (isP1 ? c.asfP1 : c.asfP2) && p->m_isShip) {
            if (!g_rt.wasShip[i]) {
                g_rt.wasShip[i] = true;
                g_rt.shipY[i] = py;
            }
            float err = (py - g_rt.shipY[i]) * dir;
            float vel = (float)p->m_yVelocity * dir;
            want[i].active = true;
            if (err < -c.asfThreshold) want[i].down = true;
            else if (err > c.asfThreshold) want[i].down = false;
            else want[i].down = vel < 0.f; // inside the band: damp the movement
        } else {
            g_rt.wasShip[i] = false;
        }

        // ---- Auto straight ufo ----
        if (c.autoStraightUfo && (isP1 ? c.asuP1 : c.asuP2) && p->m_isBird) {
            if (!g_rt.wasUfo[i]) {
                g_rt.wasUfo[i] = true;
                g_rt.ufoY[i] = py;
            }
            float target = c.asuTargetY > 0.f ? c.asuTargetY : g_rt.ufoY[i];
            float err = (py - target) * dir;
            if (err < -c.asuThreshold) swift += 1;
        } else {
            g_rt.wasUfo[i] = false;
        }

        // ---- Autoclicker ----
        bool acOn = isP1 ? c.ac1 : c.ac2;
        if (acOn) {
            int every = std::max(1, (int)std::lround(isP1 ? c.ac1Every : c.ac2Every));
            int hold = std::clamp((int)std::lround(isP1 ? c.ac1Hold : c.ac2Hold), 1, every);
            bool sw = isP1 ? c.ac1Swift : c.ac2Swift;
            if (sw) {
                swift += 1;
            } else {
                want[i].active = true;
                want[i].down = (g_rt.frame % every) < hold;
            }
        }

        // ---- Auto swift ----
        if (c.autoSwift) swift += 1;

        // ---- Moving wave gap spam ----
        if (c.waveGap && (isP1 ? c.wgP1 : c.wgP2)) {
            float thr = isP1 ? c.wgThr1 : c.wgThr2;
            if (p->m_isDart) {
                auto pr = p->getObjectRect();
                float above = FLT_MAX, below = -FLT_MAX;
                nearby(g_cache.solids, p->getPositionX(), 150.f, [&](GameObject* o) {
                    auto r = o->getObjectRect();
                    if (r.getMaxX() < pr.getMinX() - 5.f || r.getMinX() > pr.getMaxX() + 60.f) return;
                    if (r.getMinY() >= py) above = std::min(above, r.getMinY());
                    else if (r.getMaxY() <= py) below = std::max(below, r.getMaxY());
                });
                if (above < FLT_MAX && below > -FLT_MAX) {
                    float mid = (above + below) / 2.f;
                    float err = (py - mid) * dir;
                    want[i].active = true;
                    if (err < -thr) want[i].down = true;
                    else if (err > thr) want[i].down = false;
                    else want[i].down = (g_rt.frame & 1) != 0; // inside the band: spam
                }
            }
            if (isP1 ? c.wgOrb1 : c.wgOrb2) {
                swift += touching(p, {GameObjectType::GravityRing});
            }
        }

        if (swift > 0) swiftClick(l, isP1, swift);
    }

    // apply held-state wishes after swift clicks
    for (int i = 0; i < 2; i++) {
        bool isP1 = (i == 0);
        if (!isP1 && !dual) continue;
        if (want[i].active) setHeld(l, isP1, want[i].down);
        else if (g_rt.held[i]) setHeld(l, isP1, false);
    }
}

} // namespace

// ------------------------------------------------------------------
// Hooks
// ------------------------------------------------------------------
class $modify(CMBaseLayer, GJBaseGameLayer) {
    void processCommands(float dt) {
        GJBaseGameLayer::processCommands(dt);
        auto pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != this) return;
        tick(this);
    }

    // 2P control link: mirror (copy) or invert the driving player's clicks onto the other player
    void handleButton(bool down, int button, bool isPlayer1) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);

        auto& c = cfg();
        if (!c.link2P || g_inLink || button != 1) return;
        auto pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != this) return;
        if (!m_gameState.m_isDualMode) return;

        bool driverIsP1 = (c.linkDriver == 0);
        if (isPlayer1 != driverIsP1) return;

        g_inLink = true;
        GJBaseGameLayer::handleButton(c.linkMode == 1 ? down : !down, button, !isPlayer1);
        g_inLink = false;
    }
};

class $modify(CMPlayLayer, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        g_rt = Runtime{};
    }

    void onQuit() {
        g_cache.clear();
        g_rt = Runtime{};
        PlayLayer::onQuit();
    }

    // Noclip (per player). The anticheat spike still kills so the level stays valid.
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        auto& c = cfg();
        if (c.noclip && obj != m_anticheatSpike) {
            bool isP1 = (player == m_player1);
            if ((isP1 && c.ncP1) || (!isP1 && c.ncP2)) return;
        }
        PlayLayer::destroyPlayer(player, obj);
    }
};

// No orb effects
class $modify(CMRingObject, RingObject) {
    void spawnCircle() {
        if (cfg().noOrbEffects) return;
        RingObject::spawnCircle();
    }
};

class $modify(CMGameObject, GameObject) {
    void playShineEffect() {
        if (cfg().noOrbEffects) return;
        GameObject::playShineEffect();
    }
};

// Pause menu button -> opens the cheat GUI
class $modify(CMPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto spr = ButtonSprite::create("Cheats", 60, true, "bigFont.fnt", "GJ_button_06.png", 30.f, 0.55f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(CMPauseLayer::onCheatMenu));
        btn->setID("cheat-menu-button"_spr);

        if (auto menu = this->getChildByID("right-button-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        } else {
            auto m = CCMenu::create();
            m->addChild(btn);
            auto win = CCDirector::sharedDirector()->getWinSize();
            m->setPosition({win.width - 50.f, 30.f});
            this->addChild(m, 10);
        }
    }

    void onCheatMenu(CCObject*) {
        openCheatMenu();
    }
};
