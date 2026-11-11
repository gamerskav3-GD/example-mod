#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
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
        CCPoint m_pausePos;
        CCNode* m_pauseBtn = nullptr;
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

        // start the normal playtest
        m_playtestBtn->activate();

        // find the pause button: the new visible button closest to stop
        CCPoint stopWorld = ccp(0.f, 0.f);
        if (auto sp = m_playtestStopBtn->getParent()) {
            stopWorld = sp->convertToWorldSpace(m_playtestStopBtn->getPosition());
        }
        std::vector<CCMenuItemSpriteExtra*> after;
        collectButtons(this, after);
        f->m_pauseBtn = nullptr;
        float best = 1e9f;
        for (auto b : after) {
            if (b == m_playtestStopBtn || !b->isVisible()) continue;
            if (visibleBefore.count(b) > 0) continue;
            auto p = b->getParent();
            if (!p) continue;
            float d = ccpDistance(p->convertToWorldSpace(b->getPosition()), stopWorld);
            if (d < best) {
                best = d;
                f->m_pauseBtn = b;
            }
        }

        // hide the normal playtest button and our button
        m_playtestBtn->setVisible(false);
        if (f->m_practiceBtn) f->m_practiceBtn->setVisible(false);

        this->movePlaytestButtons();
    }

    void movePlaytestButtons() {
        auto f = m_fields.self();
        auto anchor = m_playbackBtn;
        auto aParent = anchor->getParent();
        auto stopParent = m_playtestStopBtn->getParent();
        if (!aParent || !stopParent) return;

        f->m_stopPos = m_playtestStopBtn->getPosition();

        auto size = anchor->getScaledContentSize();
        float dx = size.width * 1.16f;

        auto base = aParent->convertToWorldSpace(anchor->getPosition());
        float gap = size.height * 1.25f;
        if (auto testParent = m_playtestBtn->getParent()) {
            auto testWorld = testParent->convertToWorldSpace(m_playtestBtn->getPosition());
            gap = std::abs(base.y - testWorld.y);
        }

        // pause sits in the same row as the music button, stop is one row above it
        CCPoint pauseWorld = ccp(base.x + dx, base.y);
        CCPoint stopWorldTarget = ccp(base.x + dx, base.y + gap);

        m_playtestStopBtn->setPosition(stopParent->convertToNodeSpace(stopWorldTarget));

        if (f->m_pauseBtn) {
            if (auto pauseParent = f->m_pauseBtn->getParent()) {
                f->m_pausePos = f->m_pauseBtn->getPosition();
                f->m_pauseBtn->setPosition(pauseParent->convertToNodeSpace(pauseWorld));
            } else {
                f->m_pauseBtn = nullptr;
            }
        }
    }

    void restorePractice() {
        auto f = m_fields.self();
        if (!f->m_practiceActive) return;
        f->m_practiceActive = false;

        if (m_playtestStopBtn) m_playtestStopBtn->setPosition(f->m_stopPos);
        if (f->m_pauseBtn) {
            f->m_pauseBtn->setPosition(f->m_pausePos);
            f->m_pauseBtn = nullptr;
        }
        if (m_playtestBtn) m_playtestBtn->setVisible(true);
        if (f->m_practiceBtn) f->m_practiceBtn->setVisible(true);
    }

    void onStopPlaytest(CCObject* sender) {
        EditorUI::onStopPlaytest(sender);
        this->restorePractice();
    }

    void playtestStopped() {
        EditorUI::playtestStopped();
        this->restorePractice();
    }
};
