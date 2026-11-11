#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/UILayer.hpp>
#include <algorithm>
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

// put the players back into the latest saved checkpoint state
static bool respawnAtCheckpoint(LevelEditorLayer* lel) {
    auto& d = practiceData();
    if (d.p1.empty() || !lel || !lel->m_player1) return false;

    auto cp1 = d.p1.back();
    if (!cp1.data()) return false;
    lel->m_player1->loadFromCheckpoint(cp1.data());

    if (!d.p2.empty()) {
        auto cp2 = d.p2.back();
        if (cp2.data() && lel->m_player2) {
            lel->m_player2->loadFromCheckpoint(cp2.data());
        }
    }
    return true;
}

class $modify(PracticeEditorUI, EditorUI) {
    struct Fields {
        bool m_practiceActive = false;
        bool m_paused = false;
        bool m_userStop = false;
        bool m_respawning = false;
        CCPoint m_stopPos;
        CCPoint m_testPos;
        CCMenuItemSpriteExtra* m_practiceBtn = nullptr;
        CCMenu* m_checkMenu = nullptr;
        CCMenuItemSpriteExtra* m_placeBtn = nullptr;
        CCMenuItemSpriteExtra* m_removeBtn = nullptr;
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

    bool wasUserStop() {
        return m_fields->m_userStop;
    }

    void clearUserStop() {
        m_fields->m_userStop = false;
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

        if (!f->m_respawning) practiceData().clear();
        f->m_practiceActive = true;
        f->m_paused = false;
        f->m_userStop = false;

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

    // invisible tap areas on top of the game's two checkpoint buttons
    void updateCheckpointTouchButtons(bool show) {
        auto f = m_fields.self();
        if (!show) {
            if (f->m_checkMenu) f->m_checkMenu->setVisible(false);
            return;
        }

        auto ui = UILayer::get();
        if (!ui || !ui->m_checkpointMenu) return;
        auto children = ui->m_checkpointMenu->getChildren();
        if (!children) return;

        std::vector<CCMenuItem*> items;
        for (unsigned int i = 0; i < children->count(); i++) {
            auto node = static_cast<CCNode*>(children->objectAtIndex(i));
            if (auto item = typeinfo_cast<CCMenuItem*>(node)) {
                if (item->getParent()) items.push_back(item);
            }
        }
        if (items.size() < 2) return;

        auto worldPos = [](CCMenuItem* it) {
            return it->getParent()->convertToWorldSpace(it->getPosition());
        };
        auto worldSize = [](CCMenuItem* it) {
            auto parent = it->getParent();
            auto o = parent->convertToWorldSpace(ccp(0.f, 0.f));
            auto ex = parent->convertToWorldSpace(ccp(1.f, 0.f));
            auto ey = parent->convertToWorldSpace(ccp(0.f, 1.f));
            auto cs = it->getScaledContentSize();
            return CCSize(cs.width * ccpDistance(o, ex), cs.height * ccpDistance(o, ey));
        };

        // the left button places a checkpoint, the right one removes it
        std::sort(items.begin(), items.end(), [&](CCMenuItem* a, CCMenuItem* b) {
            return worldPos(a).x < worldPos(b).x;
        });
        auto placeItem = items.front();
        auto removeItem = items.back();

        if (!f->m_checkMenu) {
            f->m_checkMenu = CCMenu::create();
            f->m_checkMenu->setPosition({0.f, 0.f});
            f->m_checkMenu->setTouchPriority(-1000);
            this->addChild(f->m_checkMenu, 20);

            auto nodeA = CCNode::create();
            nodeA->setAnchorPoint({0.5f, 0.5f});
            nodeA->setContentSize(worldSize(placeItem));
            f->m_placeBtn = CCMenuItemSpriteExtra::create(
                nodeA, this, menu_selector(PracticeEditorUI::onPlaceCheckpoint)
            );
            f->m_checkMenu->addChild(f->m_placeBtn);

            auto nodeB = CCNode::create();
            nodeB->setAnchorPoint({0.5f, 0.5f});
            nodeB->setContentSize(worldSize(removeItem));
            f->m_removeBtn = CCMenuItemSpriteExtra::create(
                nodeB, this, menu_selector(PracticeEditorUI::onRemoveCheckpoint)
            );
            f->m_checkMenu->addChild(f->m_removeBtn);
        }

        f->m_placeBtn->setPosition(this->convertToNodeSpace(worldPos(placeItem)));
        f->m_removeBtn->setPosition(this->convertToNodeSpace(worldPos(removeItem)));
        f->m_checkMenu->setVisible(true);
    }

    void onPlaceCheckpoint(CCObject*) {
        auto f = m_fields.self();
        if (!f->m_practiceActive || !m_editorLayer) return;
        auto lel = m_editorLayer;
        if (!lel->m_player1) return;

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

    void onRemoveCheckpoint(CCObject*) {
        auto f = m_fields.self();
        if (!f->m_practiceActive) return;
        auto& d = practiceData();
        if (d.p1.empty()) {
            showMsg("No checkpoint to remove");
            return;
        }
        d.p1.pop_back();
        if (!d.p2.empty()) d.p2.pop_back();
        showMsg(fmt::format("Checkpoint removed ({} left)", d.p1.size()));
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
        this->updateCheckpointTouchButtons(true);
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

    // the stop button was pressed by you (so it is not a death)
    void onStopPlaytest(CCObject* sender) {
        auto f = m_fields.self();
        f->m_userStop = true;
        f->m_respawning = false;
        EditorUI::onStopPlaytest(sender);
    }

    void restorePractice() {
        auto f = m_fields.self();
        if (!f->m_practiceActive) return;
        f->m_practiceActive = false;
        f->m_paused = false;
        if (!f->m_respawning) practiceData().clear();

        this->showCheckpointButtons(false);
        this->updateCheckpointTouchButtons(false);

        if (m_playtestStopBtn) m_playtestStopBtn->setPosition(f->m_stopPos);
        if (m_playtestBtn) {
            m_playtestBtn->setPosition(f->m_testPos);
            m_playtestBtn->setVisible(true);
        }
        if (f->m_practiceBtn) f->m_practiceBtn->setVisible(true);
    }

    // death: reset the UI, then start the practice playtest again shortly
    void beginRespawn() {
        auto f = m_fields.self();
        f->m_respawning = true;
        this->restorePractice();
        this->runAction(CCSequence::create(
            CCDelayTime::create(0.15f),
            CCCallFunc::create(this, callfunc_selector(PracticeEditorUI::restartPractice)),
            nullptr
        ));
    }

    void restartPractice() {
        auto f = m_fields.self();
        if (!f->m_respawning) return; // you pressed stop in the meantime

        this->onPracticeButton(nullptr);
        if (!f->m_practiceActive) {
            f->m_respawning = false;
            practiceData().clear();
            showMsg("Could not restart the practice playtest");
            return;
        }
        this->runAction(CCSequence::create(
            CCDelayTime::create(0.1f),
            CCCallFunc::create(this, callfunc_selector(PracticeEditorUI::applyRespawnCheckpoint)),
            nullptr
        ));
    }

    void applyRespawnCheckpoint() {
        auto f = m_fields.self();
        if (!f->m_respawning) return;
        f->m_respawning = false;
        if (f->m_practiceActive) respawnAtCheckpoint(m_editorLayer);
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

    // the playtest ended: either you pressed stop, or you died
    void onStopPlaytest() {
        auto ui = m_editorUI ? static_cast<PracticeEditorUI*>(m_editorUI) : nullptr;
        bool death = ui && ui->isPracticeActive() && !ui->wasUserStop();

        LevelEditorLayer::onStopPlaytest();
        if (!ui) return;

        if (death) {
            showMsg("DEBUG: death, restarting");
            ui->beginRespawn();
        } else {
            ui->clearUserStop();
            ui->restorePractice();
        }
    }
};

// the game's own checkpoint handlers must not run in the editor (our tap areas handle it)
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
