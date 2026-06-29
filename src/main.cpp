#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>

using namespace geode::prelude;

class $modify(PlayLayer) {
    struct Fields {
        std::vector<geode::Ref<GameObject>> m_startPosObjects;
        int m_startPosIdx = 0;
        bool m_canSwitch = true;
        float m_deathX = -1.f;
    };

    void addObject(GameObject* obj) {
        PlayLayer::addObject(obj);

        if (!obj || obj->m_objectID != 31) return;

        auto* startPos = static_cast<StartPosObject*>(obj);
        if (startPos->m_startSettings && !startPos->m_startSettings->m_disableStartPos)
            m_fields->m_startPosObjects.push_back(obj);
    }

    void createObjectsFromSetupFinished() {
        PlayLayer::createObjectsFromSetupFinished();
        auto* f = m_fields.self();

        std::sort(f->m_startPosObjects.begin(), f->m_startPosObjects.end(),
            [](auto a, auto b) { return a->getPositionX() < b->getPositionX(); });

        this->setStartPosObject(nullptr);
        f->m_startPosIdx = 0;
        m_isTestMode = false;
        this->updateTestModeLabel();
    }

    void updateStartPos() {
        auto* f = m_fields.self();
        if (!f->m_canSwitch || f->m_startPosObjects.empty() || f->m_deathX < 0.f) return;

        f->m_canSwitch = false;

        int best = 0;
        for (int i = 0; i < (int)f->m_startPosObjects.size(); i++) {
            if (f->m_startPosObjects[i]->getPositionX() < f->m_deathX)
                best = i + 1;
            else
                break;
        }

        if (best == f->m_startPosIdx) {
            f->m_canSwitch = true;
            return;
        }

        f->m_startPosIdx = best;
        m_isTestMode = (best != 0);
        this->updateTestModeLabel();
        m_currentCheckpoint = nullptr;

        StartPosObject* target = (best > 0)
            ? static_cast<StartPosObject*>((GameObject*)f->m_startPosObjects[best - 1])
            : nullptr;

        this->setStartPosObject(target);

        if (m_isPracticeMode)
            this->resetLevelFromStart();

        this->resetLevel();
        this->startMusic();

        f->m_canSwitch = true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (player == m_player1)
            m_fields->m_deathX = player->getPositionX();

        PlayLayer::destroyPlayer(player, object);

        updateStartPos();
    }
};