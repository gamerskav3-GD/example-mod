#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/UILayer.hpp>

using namespace geode::prelude;

class $modify(PracticeEditorUI, EditorUI) {
    struct Fields {
        bool m_practiceActive = false;
        bool m_paused = false;
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
        if (!m_playtestBtn || !m_playtestStopBtn || !m_playbackBtn) return;

        // while the practice playtest is paused, this button resumes it
        if (f->m_practiceActive) {
            if (f->m_paused) m_playtestBtn->activate();
            return;
        }
        if (m_playtestStopBtn->isVisible()) return; // a normal playtest is running

        f->m_practiceActive = true;
        f->m_paused = false;

        // start the normal playtest (the playtest button turns into pause)
        m_playtestBtn->activate();

        this->movePlaytestButtons();
        this->applyPracticeState();
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

    void showCheckpointButtons(bool show) {
        if (auto ui = UILayer::get()) {
            ui->toggleCheckpointsMenu(show);
        }
    }

    // playing: pause icon is shown, paused: your practice icon is shown
    void applyPracticeState() {
        auto f = m_fields.self();
        if (!f->m_practiceActive) return;
        if (f->m_paused) {
            if (m_playtestBtn) m_playtestBtn->setVisible(false);
            if (f->m_practiceBtn) f->m_practiceBtn->setVisible(true);
        } else {
            if (m_playtestBtn) m_playtestBtn->setVisible(true);
            if (f->m_practiceBtn) f->m_practiceBtn->setVisible(false);
        }
        this->showCheckpointButtons(true);
    }

    void setPracticePaused(bool paused) {
        auto f = m_fields.self();
        if (!f->m_practiceActive) return;
        f->m_paused = paused;
        this->applyPracticeState();
    }

    void onPlaytest(CCObject* sender) {
        EditorUI::onPlaytest(sender);
        this->applyPracticeState();
    }

    void restorePractice() {
        auto f = m_fields.self();
        if (!f->m_practiceActive) return;
        f->m_practiceActive = false;
        f->m_paused = false;

        this->showCheckpointButtons(false);

        if (m_playtestStopBtn) m_playtestStopBtn->setPosition(f->m_stopPos);
        if (m_playtestBtn) {
            m_playtestBtn->setPosition(f->m_testPos);
            m_playtestBtn->setVisible(true);
        }
        if (f->m_practiceBtn) f->m_practiceBtn->setVisible(true);
    }
};

class $modify(PracticeLevelEditorLayer, LevelEditorLayer) {
    void onPausePlaytest() {
        LevelEditorLayer::onPausePlaytest();
        if (m_editorUI) {
            static_cast<PracticeEditorUI*>(m_editorUI)->setPracticePaused(true);
        }
    }

    void onResumePlaytest() {
        LevelEditorLayer::onResumePlaytest();
        if (m_editorUI) {
            static_cast<PracticeEditorUI*>(m_editorUI)->setPracticePaused(false);
        }
    }

    // the playtest really ended
    void onStopPlaytest() {
        LevelEditorLayer::onStopPlaytest();
        if (m_editorUI) {
            static_cast<PracticeEditorUI*>(m_editorUI)->restorePractice();
        }
    }
};

// Batch 4: the checkpoint buttons do nothing yet in the editor (Batch 5 adds this)
class $modify(PracticeUILayer, UILayer) {
    void onCheck(CCObject* sender) {
        if (m_editorMode || LevelEditorLayer::get()) return;
        UILayer::onCheck(sender);
    }

    void onDeleteCheck(CCObject* sender) {
        if (m_editorMode || LevelEditorLayer::get()) return;
        UILayer::onDeleteCheck(sender);
    }
};
