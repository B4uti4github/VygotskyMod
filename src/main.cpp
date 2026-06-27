#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

static std::string vkey(GJGameLevel* lvl, const char* suffix) {
    return fmt::format("vygotsky.{}.{}", lvl->m_levelID.value(), suffix);
}

static bool isVygotskyEnabled() {
    return Mod::get()->getSavedValue<bool>("vygotsky.enabled", true);
}

static void setVygotskyEnabled(bool v) {
    Mod::get()->setSavedValue("vygotsky.enabled", v);
}

static void saveVState(GJGameLevel* lvl, float x, float y, float rot, float percent) {
    Mod::get()->setSavedValue(vkey(lvl, "sp_x"), x);
    Mod::get()->setSavedValue(vkey(lvl, "sp_y"), y);
    Mod::get()->setSavedValue(vkey(lvl, "sp_rot"), rot);
    Mod::get()->setSavedValue(vkey(lvl, "sp_percent"), percent);
}

static bool hasVState(GJGameLevel* lvl) {
    return Mod::get()->hasSavedValue(vkey(lvl, "sp_x"));
}

static void clearVState(GJGameLevel* lvl) {
    Mod::get()->deleteSavedValue(vkey(lvl, "sp_x"));
    Mod::get()->deleteSavedValue(vkey(lvl, "sp_y"));
    Mod::get()->deleteSavedValue(vkey(lvl, "sp_rot"));
    Mod::get()->deleteSavedValue(vkey(lvl, "sp_percent"));
    Mod::get()->deleteSavedValue(vkey(lvl, "pb"));
}

class $modify(VPlayLayer, PlayLayer) {
    struct Fields {
        bool initialized = false;
        float vygotskyPB = 0.f;
        float currentAttemptBest = 0.f;
        bool wasJumpBuffered = false;
        bool passedPB = false;
        CCLabelBMFont* vygotskyLabel = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontKnow) {
        if (!PlayLayer::init(level, useReplay, dontKnow)) return false;

        auto f = m_fields;
        f->initialized = true;

        if (m_level->isPlatformer()) return true;

        f->vygotskyPB = Mod::get()->getSavedValue<float>(vkey(level, "pb"), 0.f);
        f->currentAttemptBest = 0.f;
        f->wasJumpBuffered = false;
        f->passedPB = false;
        f->vygotskyLabel = nullptr;

        if (isVygotskyEnabled() && hasVState(level)) {
            float sp = Mod::get()->getSavedValue<float>(vkey(level, "sp_percent"), f->vygotskyPB);
            f->vygotskyLabel = CCLabelBMFont::create(
                fmt::format("Vygotsky: {:.1f}%", sp).c_str(),
                "bigFont.fnt"
            );
            if (f->vygotskyLabel) {
                f->vygotskyLabel->setScale(0.35f);
                f->vygotskyLabel->setPosition({CCDirector::get()->getWinSize().width / 2.f, 28.f});
                this->addChild(f->vygotskyLabel);
            }
        }

        return true;
    }

    void resetLevel() {
        auto f = m_fields;

        if (!m_level->isPlatformer() && isVygotskyEnabled() && m_isPracticeMode && hasVState(m_level)) {
            float x = Mod::get()->getSavedValue<float>(vkey(m_level, "sp_x"), 0.f);
            float y = Mod::get()->getSavedValue<float>(vkey(m_level, "sp_y"), 0.f);
            float rot = Mod::get()->getSavedValue<float>(vkey(m_level, "sp_rot"), 0.f);

            auto sp = StartPosObject::create();
            if (sp) {
                sp->setPosition({x, y});
                sp->setRotation(rot);
                m_startPosObject = sp;
            }
        }

        PlayLayer::resetLevel();

        f->currentAttemptBest = 0.f;
        f->passedPB = false;
        f->wasJumpBuffered = false;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (!isVygotskyEnabled()) return;
        if (m_level->isPlatformer()) return;
        if (!m_player1) return;

        auto f = m_fields;
        if (f->passedPB) return;

        float percent = static_cast<float>(PlayLayer::getCurrentPercentInt());

        if (percent > f->currentAttemptBest) {
            f->currentAttemptBest = percent;
        }

        bool isJumpBuffered = m_player1->m_jumpBuffered;
        bool justJumped = isJumpBuffered && !f->wasJumpBuffered;
        f->wasJumpBuffered = isJumpBuffered;

        if (percent > f->vygotskyPB && justJumped) {
            f->passedPB = true;

            saveVState(
                m_level,
                m_player1->getPositionX(),
                m_player1->getPositionY(),
                m_player1->getRotation(),
                percent
            );
            f->vygotskyPB = percent;
            Mod::get()->setSavedValue(vkey(m_level, "pb"), percent);

            if (f->vygotskyLabel) {
                f->vygotskyLabel->setString(fmt::format("Vygotsky: {:.1f}%", percent).c_str());
            }

            auto flash = CCLayerColor::create({255, 255, 255, 90});
            if (flash) {
                this->addChild(flash);
                flash->runAction(CCSequence::create(
                    CCFadeOut::create(0.12f),
                    CCRemoveSelf::create(),
                    nullptr
                ));
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        auto f = m_fields;

        if (!m_level->isPlatformer() && isVygotskyEnabled() && f->currentAttemptBest > f->vygotskyPB) {
            f->vygotskyPB = f->currentAttemptBest;
            Mod::get()->setSavedValue(vkey(m_level, "pb"), f->currentAttemptBest);
        }

        f->currentAttemptBest = 0.f;
        f->passedPB = false;
        f->wasJumpBuffered = false;

        PlayLayer::destroyPlayer(player, obj);
    }

    void levelComplete() {
        if (!m_level->isPlatformer() && isVygotskyEnabled()) {
            clearVState(m_level);
        }
        PlayLayer::levelComplete();
    }
};

class $modify(VPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();

        auto toggleOn = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
        auto toggleOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
        auto toggleBtn = CCMenuItemToggler::create(
            toggleOff,
            toggleOn,
            this,
            menu_selector(VPauseLayer::onToggleVygotsky)
        );
        toggleBtn->setScale(0.65f);
        toggleBtn->toggle(isVygotskyEnabled());

        auto label = CCLabelBMFont::create("Vygotsky", "bigFont.fnt");
        label->setScale(0.4f);

        label->setPosition({-60.f, 0.f});
        toggleBtn->setPosition({20.f, 0.f});

        menu->addChild(label);
        menu->addChild(toggleBtn);

        auto resetBtn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_deleteBtn_001.png"),
            this,
            menu_selector(VPauseLayer::onResetVygotsky)
        );
        resetBtn->setScale(0.5f);
        resetBtn->setPosition({80.f, 0.f});
        menu->addChild(resetBtn);

        menu->setPosition({winSize.width - 100.f, winSize.height - 40.f});
        this->addChild(menu);
    }

    void onToggleVygotsky(CCObject* sender) {
        auto toggler = static_cast<CCMenuItemToggler*>(sender);
        setVygotskyEnabled(toggler->isOn());
    }

    void onResetVygotsky(CCObject*) {
        if (auto pl = PlayLayer::get()) {
            if (pl->m_level) {
                clearVState(pl->m_level);
                FLAlertLayer::create("Vygotsky", "Progress reset successfully.", "OK")->show();
            }
        }
    }
};