#include "Menu.hpp"
#include "Config.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {

// ------------------------------------------------------------------
// One option row inside an options window
// ------------------------------------------------------------------
struct Opt {
    enum Kind { Toggle, Number, Choice } kind = Toggle;
    std::string label;
    bool* b = nullptr;
    float* f = nullptr;
    float step = 1.f, lo = 0.f, hi = 1000.f;
    int* i = nullptr;
    std::vector<std::string> choices;

    static Opt toggle(std::string label, bool* b) {
        Opt o; o.kind = Toggle; o.label = std::move(label); o.b = b; return o;
    }
    static Opt number(std::string label, float* f, float step, float lo, float hi) {
        Opt o; o.kind = Number; o.label = std::move(label); o.f = f; o.step = step; o.lo = lo; o.hi = hi; return o;
    }
    static Opt choice(std::string label, int* i, std::vector<std::string> choices) {
        Opt o; o.kind = Choice; o.label = std::move(label); o.i = i; o.choices = std::move(choices); return o;
    }
};

// The small button next to a feature uses the picture from resources/optionsBtn.png
CCSprite* optionsSprite() {
    CCSprite* s = CCSprite::create("optionsBtn.png"_spr);
    if (!s) s = CCSprite::createWithSpriteFrameName("GJ_optionsBtn02_001.png");
    if (s) {
        float h = s->getContentSize().height;
        if (h > 0.f) s->setScale(28.f / h);
    }
    return s;
}

CCLabelBMFont* makeLabel(char const* text, float scale, CCPoint pos, CCPoint anchor = {0.5f, 0.5f}, float maxWidth = 0.f) {
    auto l = CCLabelBMFont::create(text, "bigFont.fnt");
    l->setColor({255, 255, 255});
    l->setAnchorPoint(anchor);
    l->setPosition(pos);
    l->setScale(scale);
    if (maxWidth > 0.f) l->limitLabelWidth(maxWidth, scale, 0.2f);
    return l;
}

// Menus must sit above the layer that swallows touches (-500)
CCMenu* makeMenu() {
    auto m = CCMenu::create();
    m->setPosition({0, 0});
    m->setTouchPriority(-501);
    return m;
}

ButtonSprite* smallBtn(char const* text, int width, float height, char const* tex) {
    return ButtonSprite::create(text, width, true, "bigFont.fnt", tex, height, 0.55f);
}

// ------------------------------------------------------------------
// Base window: black panel, white border, white text, close button
// ------------------------------------------------------------------
class BaseLayer : public CCLayerColor {
protected:
    CCSize m_size;
    CCNode* m_panel = nullptr;
    CCMenu* m_staticMenu = nullptr;

    bool initBase(CCSize size, char const* title, float titleScale) {
        if (!CCLayerColor::initWithColor({0, 0, 0, 140})) return false;
        m_size = size;
        this->setTouchEnabled(true);
        this->setKeypadEnabled(true);

        auto win = CCDirector::sharedDirector()->getWinSize();
        m_panel = CCNode::create();
        m_panel->setContentSize(size);
        m_panel->setPosition({(win.width - size.width) / 2.f, (win.height - size.height) / 2.f});
        this->addChild(m_panel);

        auto border = CCScale9Sprite::create("square02_001.png", {0, 0, 80, 80});
        border->setContentSize({size.width + 4.f, size.height + 4.f});
        border->setColor({255, 255, 255});
        border->setOpacity(255);
        border->setPosition({size.width / 2.f, size.height / 2.f});
        m_panel->addChild(border, -2);

        auto bg = CCScale9Sprite::create("square02_001.png", {0, 0, 80, 80});
        bg->setContentSize(size);
        bg->setColor({0, 0, 0});
        bg->setOpacity(255);
        bg->setPosition({size.width / 2.f, size.height / 2.f});
        m_panel->addChild(bg, -1);

        m_panel->addChild(makeLabel(title, titleScale, {size.width / 2.f, size.height - 24.f}, {0.5f, 0.5f}, size.width - 90.f));

        m_staticMenu = makeMenu();
        m_panel->addChild(m_staticMenu);

        auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        closeSpr->setScale(0.7f);
        auto closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(BaseLayer::onClose));
        closeBtn->setPosition({16.f, size.height - 16.f});
        m_staticMenu->addChild(closeBtn);
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -500, true);
    }
    bool ccTouchBegan(CCTouch*, CCEvent*) override { return true; } // swallow touches behind the menu
    void keyBackClicked() override { this->onClose(nullptr); }

    void onClose(CCObject*) {
        cfg().save();
        this->setKeypadEnabled(false);
        this->removeFromParentAndCleanup(true);
    }
};

// ------------------------------------------------------------------
// Options window (opened by the small button next to a feature)
// ------------------------------------------------------------------
class OptionsLayer : public BaseLayer {
    std::vector<Opt> m_opts;
    std::vector<CCLabelBMFont*> m_values;
    std::vector<std::vector<ButtonSprite*>> m_choiceSprites;

    bool init(std::string const& title, std::vector<Opt> opts) {
        m_opts = std::move(opts);
        float h = 70.f + 38.f * m_opts.size();
        if (!initBase({380.f, h}, title.c_str(), 0.7f)) return false;
        build();
        return true;
    }

    static std::string fmtNum(float v) { return fmt::format("{}", v); }

    void build() {
        auto menu = makeMenu();
        m_panel->addChild(menu);
        m_values.assign(m_opts.size(), nullptr);
        m_choiceSprites.assign(m_opts.size(), {});

        for (size_t i = 0; i < m_opts.size(); i++) {
            auto& o = m_opts[i];
            float y = m_size.height - 62.f - 38.f * i;

            if (o.kind == Opt::Toggle) {
                auto t = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(OptionsLayer::onToggle), 0.7f);
                t->toggle(*o.b);
                t->setTag((int)i);
                t->setPosition({32.f, y});
                menu->addChild(t);
                m_panel->addChild(makeLabel(o.label.c_str(), 0.5f, {58.f, y}, {0.f, 0.5f}, 290.f));
            } else if (o.kind == Opt::Number) {
                m_panel->addChild(makeLabel(o.label.c_str(), 0.5f, {20.f, y}, {0.f, 0.5f}, 140.f));
                float xs[4] = {178.f, 212.f, 288.f, 322.f};
                char const* txt[4] = {"--", "-", "+", "++"};
                for (int k = 0; k < 4; k++) {
                    auto btn = CCMenuItemSpriteExtra::create(smallBtn(txt[k], 28, 26.f, "GJ_button_04.png"), this,
                                                             menu_selector(OptionsLayer::onNumber));
                    btn->setTag((int)i * 10 + k);
                    btn->setPosition({xs[k], y});
                    menu->addChild(btn);
                }
                auto v = makeLabel(fmtNum(*o.f).c_str(), 0.5f, {250.f, y});
                m_panel->addChild(v);
                m_values[i] = v;
            } else { // Choice
                m_panel->addChild(makeLabel(o.label.c_str(), 0.5f, {20.f, y}, {0.f, 0.5f}, 120.f));
                for (size_t k = 0; k < o.choices.size(); k++) {
                    bool sel = (*o.i == (int)k);
                    auto spr = smallBtn(o.choices[k].c_str(), 80, 26.f, sel ? "GJ_button_01.png" : "GJ_button_04.png");
                    auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(OptionsLayer::onChoice));
                    btn->setTag((int)i * 10 + (int)k);
                    btn->setPosition({205.f + 92.f * k, y});
                    menu->addChild(btn);
                    m_choiceSprites[i].push_back(spr);
                }
            }
        }
    }

    void onToggle(CCObject* s) {
        auto t = static_cast<CCMenuItemToggler*>(s);
        *m_opts[t->getTag()].b = !t->isToggled();
    }

    void onNumber(CCObject* s) {
        int tag = static_cast<CCNode*>(s)->getTag();
        size_t row = tag / 10;
        int k = tag % 10;
        auto& o = m_opts[row];
        float d = (k == 0) ? -o.step * 10.f : (k == 1) ? -o.step : (k == 2) ? o.step : o.step * 10.f;
        float nv = std::clamp(*o.f + d, o.lo, o.hi);
        *o.f = std::round(nv * 100.f) / 100.f;
        if (m_values[row]) m_values[row]->setString(fmtNum(*o.f).c_str());
    }

    void onChoice(CCObject* s) {
        int tag = static_cast<CCNode*>(s)->getTag();
        size_t row = tag / 10;
        int k = tag % 10;
        *m_opts[row].i = k;
        for (size_t j = 0; j < m_choiceSprites[row].size(); j++) {
            m_choiceSprites[row][j]->updateBGImage(j == (size_t)k ? "GJ_button_01.png" : "GJ_button_04.png");
        }
    }

public:
    static void open(std::string const& title, std::vector<Opt> opts) {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (!scene) return;
        auto ret = new OptionsLayer();
        if (ret && ret->init(title, std::move(opts))) {
            ret->autorelease();
            scene->addChild(ret, 10001);
        } else {
            CC_SAFE_DELETE(ret);
        }
    }
};

// ------------------------------------------------------------------
// Main cheat menu (section: Gameplay)
// ------------------------------------------------------------------
class CheatMenu : public BaseLayer {
    struct Feature {
        std::string name;
        bool* flag;
        std::function<void()> options; // empty = no options button
    };

    std::vector<Feature> m_features;
    int m_page = 0;
    static constexpr int PER_PAGE = 6;
    CCNode* m_rows = nullptr;
    CCLabelBMFont* m_pageLabel = nullptr;

    int pageCount() const { return ((int)m_features.size() + PER_PAGE - 1) / PER_PAGE; }

    void buildFeatures() {
        auto& c = cfg();
        auto players = [](bool* a, bool* b) {
            return std::vector<Opt>{Opt::toggle("Player 1", a), Opt::toggle("Player 2", b)};
        };

        m_features = {
            {"Click Green Dash Orbs", &c.clickGreenDash,
             [&c, players] { OptionsLayer::open("Green Dash Orbs", players(&c.gdP1, &c.gdP2)); }},

            {"No Orb Effects", &c.noOrbEffects, {}},
            {"Click Black Orbs", &c.clickBlackOrb, {}},
            {"Easy Click Black Orb UFO", &c.easyClickBlackUfo, {}},
            {"Click Jump Pads", &c.clickJumpPads, {}},
            {"Click Gravity Pads", &c.clickGravityPads, {}},

            {"Maintain Gravity", &c.maintainGravity,
             [&c, players] { OptionsLayer::open("Maintain Gravity", players(&c.mgP1, &c.mgP2)); }},

            {"Auto Straight Fly", &c.autoStraightFly,
             [&c, players] {
                 auto v = players(&c.asfP1, &c.asfP2);
                 v.push_back(Opt::number("Threshold", &c.asfThreshold, 0.5f, 0.f, 200.f));
                 OptionsLayer::open("Auto Straight Fly", v);
             }},

            {"Auto Straight UFO", &c.autoStraightUfo,
             [&c, players] {
                 auto v = players(&c.asuP1, &c.asuP2);
                 v.push_back(Opt::number("Threshold", &c.asuThreshold, 1.f, 0.f, 200.f));
                 v.push_back(Opt::number("Y Position Target", &c.asuTargetY, 5.f, 0.f, 5000.f));
                 OptionsLayer::open("Auto Straight UFO", v);
             }},

            {"AutoClicker Player 1", &c.ac1,
             [&c] {
                 OptionsLayer::open("AutoClicker Player 1", {
                     Opt::number("Click Every (frames)", &c.ac1Every, 1.f, 1.f, 600.f),
                     Opt::number("Hold For (frames)", &c.ac1Hold, 1.f, 1.f, 600.f),
                     Opt::toggle("Swift", &c.ac1Swift),
                 });
             }},

            {"AutoClicker Player 2", &c.ac2,
             [&c] {
                 OptionsLayer::open("AutoClicker Player 2", {
                     Opt::number("Click Every (frames)", &c.ac2Every, 1.f, 1.f, 600.f),
                     Opt::number("Hold For (frames)", &c.ac2Hold, 1.f, 1.f, 600.f),
                     Opt::toggle("Swift", &c.ac2Swift),
                 });
             }},

            {"Noclip", &c.noclip,
             [&c, players] { OptionsLayer::open("Noclip", players(&c.ncP1, &c.ncP2)); }},

            {"Auto Swift", &c.autoSwift, {}},

            {"2P Control Link", &c.link2P,
             [&c] {
                 OptionsLayer::open("2P Control Link", {
                     Opt::choice("Mode", &c.linkMode, {"Invert", "Copy"}),
                     Opt::choice("Driving Side", &c.linkDriver, {"Player 1", "Player 2"}),
                 });
             }},

            {"Moving Wave Gap Spam", &c.waveGap,
             [&c] {
                 OptionsLayer::open("Moving Wave Gap Spam", {
                     Opt::toggle("Player 1", &c.wgP1),
                     Opt::number("P1 Threshold", &c.wgThr1, 0.5f, 0.f, 100.f),
                     Opt::toggle("P1 Blue Orb", &c.wgOrb1),
                     Opt::toggle("Player 2", &c.wgP2),
                     Opt::number("P2 Threshold", &c.wgThr2, 0.5f, 0.f, 100.f),
                     Opt::toggle("P2 Blue Orb", &c.wgOrb2),
                 });
             }},
        };
    }

    bool init() {
        if (!initBase({400.f, 320.f}, "helloworld1246's utils", 0.7f)) return false;
        buildFeatures();

        m_panel->addChild(makeLabel("Gameplay", 0.6f, {m_size.width / 2.f, m_size.height - 52.f}));

        // page arrows
        auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        leftSpr->setScale(0.7f);
        auto leftBtn = CCMenuItemSpriteExtra::create(leftSpr, this, menu_selector(CheatMenu::onPrev));
        leftBtn->setPosition({m_size.width / 2.f - 60.f, 22.f});
        m_staticMenu->addChild(leftBtn);

        auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        rightSpr->setFlipX(true);
        rightSpr->setScale(0.7f);
        auto rightBtn = CCMenuItemSpriteExtra::create(rightSpr, this, menu_selector(CheatMenu::onNext));
        rightBtn->setPosition({m_size.width / 2.f + 60.f, 22.f});
        m_staticMenu->addChild(rightBtn);

        m_pageLabel = makeLabel("", 0.5f, {m_size.width / 2.f, 22.f});
        m_panel->addChild(m_pageLabel);

        showPage();
        return true;
    }

    void showPage() {
        if (m_rows) m_rows->removeFromParent();
        m_rows = CCNode::create();
        m_panel->addChild(m_rows);
        auto menu = makeMenu();
        m_rows->addChild(menu);

        int start = m_page * PER_PAGE;
        int end = std::min((int)m_features.size(), start + PER_PAGE);
        for (int idx = start; idx < end; idx++) {
            auto& f = m_features[idx];
            float y = m_size.height - 88.f - 36.f * (idx - start);

            auto t = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(CheatMenu::onToggle), 0.7f);
            t->toggle(*f.flag);
            t->setTag(idx);
            t->setPosition({32.f, y});
            menu->addChild(t);

            m_rows->addChild(makeLabel(f.name.c_str(), 0.5f, {58.f, y}, {0.f, 0.5f}, 270.f));

            if (f.options) {
                auto btn = CCMenuItemSpriteExtra::create(optionsSprite(), this, menu_selector(CheatMenu::onOptions));
                btn->setTag(idx);
                btn->setPosition({m_size.width - 34.f, y});
                menu->addChild(btn);
            }
        }
        m_pageLabel->setString(fmt::format("{}/{}", m_page + 1, pageCount()).c_str());
    }

    void onToggle(CCObject* s) {
        auto t = static_cast<CCMenuItemToggler*>(s);
        *m_features[t->getTag()].flag = !t->isToggled();
    }

    void onOptions(CCObject* s) {
        auto& f = m_features[static_cast<CCNode*>(s)->getTag()];
        if (f.options) f.options();
    }

    void onPrev(CCObject*) {
        m_page = (m_page + pageCount() - 1) % pageCount();
        showPage();
    }
    void onNext(CCObject*) {
        m_page = (m_page + 1) % pageCount();
        showPage();
    }

public:
    static void open() {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (!scene) return;
        if (scene->getChildByID("cheat-menu"_spr)) return;
        auto ret = new CheatMenu();
        if (ret && ret->init()) {
            ret->autorelease();
            ret->setID("cheat-menu"_spr);
            scene->addChild(ret, 10000);
        } else {
            CC_SAFE_DELETE(ret);
        }
    }
};

} // namespace

void openCheatMenu() {
    CheatMenu::open();
}
