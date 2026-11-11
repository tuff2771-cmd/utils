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

        auto closeSpr = CCSprite::create
