#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>

using namespace geode::prelude;

class $modify(PracticeEditorUI, EditorUI) {
    struct Fields {
        bool m_practiceActive = false;
        bool m_positionsSaved = false;
        CCPoint m_stopPos;
        CCPoint m_pausePos;
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
        if (!m_playtestBtn || !m_playtestStopBtn || !m_playtestPauseBtn || !m_playbackBtn) return;
        if (m_playtestStopBtn->isVisible()) return; // a playtest is already running

        f->m_practiceActive = true;

        // start the normal playtest
        m_playtestBtn->activate();

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
        auto pauseParent = m_playtestPauseBtn->getParent();
        if (!aParent || !stopParent || !pauseParent) return;

        if (!f->m_positionsSaved) {
            f->m_stopPos = m_playtestStopBtn->getPosition();
            f->m_pausePos = m_playtestPauseBtn->getPosition();
            f->m_positionsSaved = true;
        }

        auto size = anchor->getScaledContentSize();
        float dx = size.width * 1.16f;

        auto base = aParent->convertToWorldSpace(anchor->getPosition());
        float gap = size.height * 1.25f;
        auto testParent = m_playtestBtn->getParent();
        if (testParent) {
            auto testWorld = testParent->convertToWorldSpace(m_playtestBtn->getPosition());
            gap = std::abs(base.y - testWorld.y);
        }

        // pause sits in the same row as the music button, stop is one row above it
        CCPoint pauseWorld = ccp(base.x + dx, base.y);
        CCPoint stopWorld = ccp(base.x + dx, base.y + gap);

        m_playtestPauseBtn->setPosition(pauseParent->convertToNodeSpace(pauseWorld));
        m_playtestStopBtn->setPosition(stopParent->convertToNodeSpace(stopWorld));
    }

    void restorePractice() {
        auto f = m_fields.self();
        if (!f->m_practiceActive) return;
        f->m_practiceActive = false;

        if (f->m_positionsSaved) {
            if (m_playtestStopBtn) m_playtestStopBtn->setPosition(f->m_stopPos);
            if (m_playtestPauseBtn) m_playtestPauseBtn->setPosition(f->m_pausePos);
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
