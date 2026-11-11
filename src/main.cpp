#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <unordered_set>
#include <vector>

using namespace geode::prelude;

static void collectButtons(CCNode* node, std::vector<CCMenuItemSpriteExtra*>& out) {
    if (!node) return;
    if (auto btn = typeinfo_cast<CCMenuItemSpriteExtra*>(node)) out.push_back(btn);
    auto children = node->getChildren();
    if (!children) return;
    for (unsigned int i = 0; i < children->count(); i++) {
        collectButtons(static_cast<CCNode*>(children->objectAtIndex(i)), out);
    }
}

class $modify(PracticeEditorUI, EditorUI) {
    struct Fields {
        bool m_practiceActive = false;
        CCPoint m_stopPos;
        CCPoint m_testPos;
        CCMenuItemSpriteExtra* m_practiceBtn = nullptr;
    };

    bool init(LevelEditorLayer* editor) {
        if (!EditorUI::init(editor)) return false;

        auto anchor = m_playbackBtn;
        if (!anchor || !anchor->getParent()) return true;

        auto sprite = CCSprite::create("button_clean.png"_spr);
        if (!sprite) return true;
        auto size = anchor->getScaledContentSize();
        sprite->setScale(size.width / sprite->getContentSize().width);

        auto btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(PracticeEditorUI::onPracticeButton)
        );
        btn->setID("practice-playtest-button"_spr);
        m_fields->m_practiceBtn = btn;

        auto world = anchor->getParent()->convertToWorldSpace(
            anchor->getPosition() + ccp(size.width * 1.16f, 0.f)
        );
        btn->setPosition(this->convertToNodeSpace(world));

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->addChild(btn);
        this->addChild(menu, 10);
        return true;
    }

    void onPracticeButton(CCObject*) {
        auto f = m_fields.self();
        if (f->m_practiceActive) return;
        if (!m_playtestBtn || !m_playtestStopBtn || !m_playbackBtn) return;
        if (m_playtestStopBtn->isVisible()) return; // a playtest is already running

        // remember which buttons are visible before the playtest starts
        std::vector<CCMenuItemSpriteExtra*> before;
        collectButtons(this, before);
        std::unordered_set<CCMenuItemSpriteExtra*> visibleBefore;
        for (auto b : before) {
            if (b->isVisible()) visibleBefore.insert(b);
        }

        f->m_practiceActive = true;

        // start the normal playtest (the playtest button turns into pause)
        m_playtestBtn->activate();

        // hide extra buttons the playtest shows (like START POS)
        std::vector<CCMenuItemSpriteExtra*> after;
        collectButtons(this, after);
        for (auto b : after) {
            if (b == m_playtestStopBtn || b == m_playtestBtn || b == f->m_practiceBtn) continue;
            if (b->isVisible() && visibleBefore.count(b) == 0) b->setVisible(false);
        }

        // hide our button, its spot is taken by pause
        if (f->m_practiceBtn) f->m_practiceBtn->setVisible(false);

        this->movePlaytestButtons();
    }

    void movePlaytestButtons() {
        auto f = m_fields.self();
        auto anchor = m_playbackBtn;
        auto aParent = anchor->getParent();
        auto stopParent = m_playtestStopBtn->getParent();
        auto testParent = m_playtestBtn->getParent();
        if (!aParent || !stopParent || !testParent) return;

        f->m_stopPos = m_playtestStopBtn->getPosition();
        f->m_testPos = m_playtestBtn->getPosition();

        auto size = anchor->getScaledContentSize();
        float dx = size.width * 1.16f;

        auto base = aParent->convertToWorldSpace(anchor->getPosition());
        auto testWorld = testParent->convertToWorldSpace(m_playtestBtn->getPosition());
        float gap = std::abs(base.y - testWorld.y);

        // pause goes where your button is, stop goes one row above it
        CCPoint pauseWorld = ccp(base.x + dx, base.y);
        CCPoint stopWorld = ccp(base.x + dx, base.y + gap);

        m_playtestBtn->setPosition(testParent->convertToNodeSpace(pauseWorld));
        m_playtestStopBtn->setPosition(stopParent->convertToNodeSpace(stopWorld));
    }

    void restorePractice() {
        auto f = m_fields.self();
        if (!f->m_practiceActive) return;
        f->m_practiceActive = false;

        if (m_playtestStopBtn) m_playtestStopBtn->setPosition(f->m_stopPos);
        if (m_playtestBtn) m_playtestBtn->setPosition(f->m_testPos);
        if (f->m_practiceBtn) f->m_practiceBtn->setVisible(true);
    }
};

// the playtest really ended (stop button, or any other way it stops)
class $modify(PracticeLevelEditorLayer, LevelEditorLayer) {
    void onStopPlaytest() {
        LevelEditorLayer::onStopPlaytest();
        if (m_editorUI) {
            static_cast<PracticeEditorUI*>(m_editorUI)->restorePractice();
        }
    }
};
