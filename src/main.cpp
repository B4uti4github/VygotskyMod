#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>

using namespace geode::prelude;

class $modify(PlayLayer) {
    struct Fields {
        std::vector<geode::Ref<GameObject>> m_startPosObjects;
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
        m_isTestMode = false;
        this->updateTestModeLabel();
    }

    void resetLevel() {
        auto* f = m_fields.self();

        if (f->m_deathX >= 0.f && !f->m_startPosObjects.empty()) {
            StartPosObject* best = nullptr;
            for (auto& ref : f->m_startPosObjects) {
                if (ref->getPositionX() < f->m_deathX)
                    best = static_cast<StartPosObject*>((GameObject*)ref);
                else
                    break;
            }

            if (m_startPos != best) {
                this->setStartPosObject(best);
                m_isTestMode = (best != nullptr);
                this->updateTestModeLabel();
            }
        }

        PlayLayer::resetLevel();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (player == m_player1)
            m_fields->m_deathX = player->getPositionX();

        PlayLayer::destroyPlayer(player, object);
    }
};