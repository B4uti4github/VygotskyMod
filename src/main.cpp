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
    return Mod::get()->getSavedValue<float>(vkey(lvl, "pb"), 0.f) > 0.f;
}

static void clearVState(GJGameLevel* lvl) {
    Mod::get()->setSavedValue(vkey(lvl, "sp_x"), 0.f);
    Mod::get()->setSavedValue(vkey(lvl, "sp_y"), 0.f);
    Mod::get()->setSavedValue(vkey(lvl, "sp_rot"), 0.f);
    Mod::get()->setSavedValue(vkey(lvl, "sp_percent"), 0.f);
    Mod::get()->setSavedValue(vkey(lvl, "pb"), 0.f);
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

        m_fields->initialized = true;

        if (m_level->isPlatformer()) return true;

        m_fields->vygotskyPB = Mod::get()->getSavedValue<float>(vkey(level, "pb"), 0.f);
        m_fields->currentAttemptBest = 0.f;
        m_fields->wasJumpBuffered = false;
        m_fields->passedPB = false;
        m_fields->vygotskyLabel = nullptr;

        if (isVygotskyEnabled() && hasVState(level)) {
            float sp = Mod::get()->getSavedValue<float>(vkey(level, "sp_percent"), m_fields->vygotskyPB);
            m_fields->vygotskyLabel = CCLabelBMFont::create(
                fmt::format("Vygotsky: {:.1f}%", sp).c_str(),
                "bigFont.fnt"
            );
            if (m_fields->vygotskyLabel) {
                m_fields->vygotskyLabel->setScale(0.35f);
                m_fields->vygotskyLabel->setPosition({CCDirector::get()->getWinSize().width / 2.f, 28.f});
                this->addChild(m_fields->vygotskyLabel);
            }
        }

        return true;
    }

    void resetLevel() {
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

        m_fields->currentAttemptBest = 0.f;
        m_fields->passedPB = false;
        m_fields->wasJumpBuffered = false;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (!isVygotskyEnabled()) return;
        if (m_level->isPlatformer()) return;
        if (!m_player1) return;

        if (m_fields->passedPB) return;

        float percent = static_cast<float>(PlayLayer::getCurrentPercentInt());

        if (percent > m_fields->currentAttemptBest) {
            m_fields->currentAttemptBest = percent;
        }

        bool isJumpBuffered = m_player1->m_jumpBuffered;
        bool justJumped = isJumpBuffered && !m_fields->wasJumpBuffered;
        m_fields->wasJumpBuffered = isJumpBuffered;

        if (percent > m_fields->vygotskyPB && justJumped) {
            m_fields->passedPB = true;

            saveVState(
                m_level,
                m_player1->getPositionX(),
                m_player1->getPositionY(),
                m_player1->getRotation(),
                percent
            );
            m_fields->vygotskyPB = percent;
            Mod::get()->setSavedValue(vkey(m_level, "pb"), percent);

            if (m_fields->vygotskyLabel) {
                m_fields->vygotskyLabel->setString(fmt::format("Vygotsky: {:.1f}%", percent).c_str());
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
        if (!m_level->isPlatformer() && isVygotskyEnabled() && m_fields->currentAttemptBest > m_fields->vygotskyPB) {
            m_fields->vygotskyPB = m_fields->currentAttemptBest;
            Mod::get()->setSavedValue(vkey(m_level, "pb"), m_fields->currentAttemptBest);
        }

        m_fields->currentAttemptBest = 0.f;
        m_fields->passedPB = false;
        m_fields->wasJumpBuffered = false;

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