#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/UILayer.hpp>
#include <vector>

using namespace geode::prelude;

struct PracticeData {
    std::vector<Ref<PlayerCheckpoint>> p1;
    std::vector<Ref<PlayerCheckpoint>> p2;
    void clear() {
        p1.clear();
        p2.clear();
    }
};

static PracticeData& practiceData() {
    static PracticeData data;
    return data;
}

static void showMsg(std::string const& text) {
    Notification::create(text, NotificationIcon::None, 1.f)->show();
}

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

    bool isPracticeActive() {
        return m_fields->m_practiceActive;
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

        practiceData().clear();
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
        practiceData().clear();

        this->showCheckpointButtons(false);

        if (m_playtestStopBtn) m_playtestStopBtn->setPosition(f->m_stopPos);
        if (m_playtestBtn) {
            m_playtestBtn->setPosition(f->m_testPos);
            m_playtestBtn->setVisible(true);
        }
        if (f->m_practiceBtn) f->m_practiceBtn->setVisible(true);
    }
};

static PracticeEditorUI* practiceUI() {
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_editorUI) return nullptr;
    return static_cast<PracticeEditorUI*>(lel->m_editorUI);
}

static void placePracticeCheckpoint() {
    auto ui = practiceUI();
    if (!ui || !ui->isPracticeActive()) return;
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_player1) return;

    auto cp1 = PlayerCheckpoint::create();
    if (!cp1) return;
    lel->m_player1->saveToCheckpoint(cp1);

    PlayerCheckpoint* cp2 = nullptr;
    if (lel->m_player2 && lel->m_player2->isVisible()) {
        cp2 = PlayerCheckpoint::create();
        if (cp2) lel->m_player2->saveToCheckpoint(cp2);
    }

    auto& d = practiceData();
    d.p1.push_back(Ref<PlayerCheckpoint>(cp1));
    d.p2.push_back(Ref<PlayerCheckpoint>(cp2));
    showMsg(fmt::format("Checkpoint {} placed", d.p1.size()));
}

static void removePracticeCheckpoint() {
    auto ui = practiceUI();
    if (!ui || !ui->isPracticeActive()) return;
    auto& d = practiceData();
    if (d.p1.empty()) {
        showMsg("No checkpoint to remove");
        return;
    }
    d.p1.pop_back();
    if (!d.p2.empty()) d.p2.pop_back();
    showMsg(fmt::format("Checkpoint removed ({} left)", d.p1.size()));
}

static bool respawnAtCheckpoint(LevelEditorLayer* lel) {
    auto& d = practiceData();
    if (d.p1.empty() || !lel || !lel->m_player1) return false;

    auto cp1 = d.p1.back();
    if (!cp1.data()) return false;
    lel->m_player1->loadFromCheckpoint(cp1.data());
    lel->m_player1->m_isDead = false;
    lel->m_player1->setVisible(true);

    auto cp2 = d.p2.empty() ? Ref<PlayerCheckpoint>() : d.p2.back();
    if (cp2.data() && lel->m_player2) {
        lel->m_player2->loadFromCheckpoint(cp2.data());
        lel->m_player2->m_isDead = false;
    }
    return true;
}

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
            auto ui = static_cast<PracticeEditorUI*>(m_editorUI);
            if (ui->isPracticeActive()) showMsg("DEBUG: playtest stopped");
            ui->restorePractice();
        }
    }

    // death: respawn at the latest checkpoint instead of stopping
    void playerTookDamage(PlayerObject* player) {
        auto ui = m_editorUI ? static_cast<PracticeEditorUI*>(m_editorUI) : nullptr;
        if (ui && ui->isPracticeActive()) {
            if (respawnAtCheckpoint(this)) {
                showMsg("DEBUG: died, respawned at checkpoint");
                return;
            }
            showMsg("DEBUG: died, no checkpoint");
        }
        LevelEditorLayer::playerTookDamage(player);
    }
};

// the game's checkpoint buttons now place and remove our checkpoints in the editor
class $modify(PracticeUILayer, UILayer) {
    void onCheck(CCObject* sender) {
        if (m_editorMode || LevelEditorLayer::get()) {
            placePracticeCheckpoint();
            return;
        }
        UILayer::onCheck(sender);
    }

    void onDeleteCheck(CCObject* sender) {
        if (m_editorMode || LevelEditorLayer::get()) {
            removePracticeCheckpoint();
            return;
        }
        UILayer::onDeleteCheck(sender);
    }
};
