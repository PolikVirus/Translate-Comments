#include <Geode/Geode.hpp>
#include <Geode/modify/CommentCell.hpp>
#include <Geode/modify/IDManager.hpp>
#include <Geode/binding/GJComment.hpp>
#include <Geode/binding/TextArea.hpp>
#include <Geode/utils/web.hpp>
#include <matjson.hpp>

#include <chrono>
#include <unordered_map>
#include <vector>
#include <array>
#include <cfloat>
#include <cmath>
#include <algorithm>

using namespace geode::prelude;

namespace {
    std::unordered_map<int, std::string> s_cache;

    std::string makeNodeID(std::string_view local) {
        return Mod::get()->getID() + "/" + std::string(local);
    }

    std::string parseTranslation(matjson::Value const& root) {
        if (!root.isArray()) return "";

        auto const& chunks = root[0];
        if (!chunks.isArray()) return "";

        std::string out;
        for (auto const& chunk : chunks) {
            if (!chunk.isArray()) continue;
            out += chunk[0].asString().unwrapOr("");
        }
        return out;
    }

    bool isInSubtree(CCNode* node, CCNode* root) {
        if (!node || !root) return false;
        for (auto p = node; p; p = p->getParent()) {
            if (p == root) return true;
        }
        return false;
    }

    // Expand min/max X (in `menu` node space) for `n`'s *visual* subtree.
    // Works even if CCMenuItemSpriteExtra has 0 contentSize, because its sprite child has size.
    void expandVisualBoundsX(CCNode* n, CCNode* menu, float& minX, float& maxX, bool& found) {
        if (!n || !menu) return;

        auto sz = n->getContentSize();
        if (sz.width > 0.5f && sz.height > 0.5f) {
            auto ap = n->getAnchorPoint();
            float l = -ap.x * sz.width;
            float r = (1.f - ap.x) * sz.width;
            float b = -ap.y * sz.height;
            float t = (1.f - ap.y) * sz.height;

            std::array<CCPoint, 4> pts = { CCPoint{l,b}, CCPoint{l,t}, CCPoint{r,b}, CCPoint{r,t} };
            for (auto const& p : pts) {
                auto w = n->convertToWorldSpace(p);
                auto m = menu->convertToNodeSpace(w);
                minX = std::min(minX, m.x);
                maxX = std::max(maxX, m.x);
                found = true;
            }
        }

        for (auto c : n->getChildrenExt<CCNode*>()) {
            if (c) expandVisualBoundsX(c, menu, minX, maxX, found);
        }
    }

    float visualLeftEdge(CCNode* node, CCNode* menu) {
        float minX = FLT_MAX, maxX = -FLT_MAX;
        bool found = false;
        expandVisualBoundsX(node, menu, minX, maxX, found);
        return found ? minX : node->getPositionX();
    }

    float visualRightEdge(CCNode* node, CCNode* menu) {
        float minX = FLT_MAX, maxX = -FLT_MAX;
        bool found = false;
        expandVisualBoundsX(node, menu, minX, maxX, found);
        return found ? maxX : node->getPositionX();
    }

    // If older versions created helper menus and moved buttons, restore them (safe no-op otherwise)
    void restoreButtonsFromOldMenu(CCMenu* mainMenu) {
        if (!mainMenu) return;

        auto tryRestore = [&](std::string const& menuID) {
            auto oldMenu = typeinfo_cast<CCMenu*>(mainMenu->getChildByIDRecursive(menuID));
            if (!oldMenu) return;

            std::vector<CCNode*> kids;
            for (auto n : oldMenu->getChildrenExt<CCNode*>()) if (n) kids.push_back(n);

            for (auto n : kids) {
                if (!n) continue;
                if (n->getID() == makeNodeID("translate-button")) continue;

                auto world = oldMenu->convertToWorldSpace(n->getPosition());
                auto inMain = mainMenu->convertToNodeSpace(world);

                n->retain();
                n->removeFromParentAndCleanup(false);
                mainMenu->addChild(n);
                n->setPosition(inMain);
                n->release();
            }

            oldMenu->removeFromParent();
        };

        tryRestore(makeNodeID("translate-actions-menu"));
        tryRestore(makeNodeID("actions-menu"));
    }
}

class $modify(TranslatingCommentCell, CommentCell) {
    struct Fields {
        TaskHolder<web::WebResponse> m_translateTask;
    };

    void loadFromComment(GJComment* comment) {
        CommentCell::loadFromComment(comment);
        NodeIDs::provideFor(this);

        this->installTranslateButton();
        this->applyCachedTranslation();
    }

    void installTranslateButton() {
        if (!m_mainLayer || !m_comment || m_comment->m_commentDeleted) return;

        auto menu = typeinfo_cast<CCMenu*>(m_mainLayer->getChildByIDRecursive("main-menu"));
        if (!menu) return;

        restoreButtonsFromOldMenu(menu);

        auto btnID = makeNodeID("translate-button");
        if (auto old = menu->getChildByIDRecursive(btnID)) old->removeFromParent();

        auto icon = CCSprite::create("translate.png"_spr);
        if (!icon) return;
        icon->setScale(0.65f);

        auto btn = CCMenuItemSpriteExtra::create(
            icon, this, menu_selector(TranslatingCommentCell::onTranslate)
        );
        btn->setID(btnID);
        btn->setSizeMult(1.2f);

        // BetterInfo-style: add directly to main-menu
        menu->addChild(btn);
        this->positionTranslateButton(menu, btn);
    }

    void positionTranslateButton(CCMenu* menu, CCNode* btn) {
        if (!menu || !btn) return;

        auto userMenu = menu->getChildByIDRecursive("user-menu");
        auto usernameMenu = menu->getChildByIDRecursive("username-menu");

        // Use the action-row Y (like/delete/spam/level)
        float baseY = 0.f;
        if (auto like = menu->getChildByIDRecursive("like-button")) baseY = like->getPositionY();
        else if (auto del = menu->getChildByIDRecursive("delete-button")) baseY = del->getPositionY();
        else if (auto spam = menu->getChildByIDRecursive("spam-button")) baseY = spam->getPositionY();
        else if (auto lvl = menu->getChildByIDRecursive("level-button")) baseY = lvl->getPositionY();
        else baseY = btn->getPositionY();

        // Only consider nodes on (roughly) the same row so we don't react to unrelated sprites.
        constexpr float yTol = 10.f;

        float minLeft = FLT_MAX;

        for (auto node : menu->getChildrenExt<CCNode*>()) {
            if (!node) continue;
            if (node == btn) continue;
            if (!node->isVisible()) continue;

            if (isInSubtree(node, userMenu)) continue;
            if (isInSubtree(node, usernameMenu)) continue;

            // Only consider clickable buttons (BetterInfo + vanilla buttons are menu items)
            if (!typeinfo_cast<CCMenuItem*>(node)) continue;

            if (std::abs(node->getPositionY() - baseY) > yTol) continue;

            float le = visualLeftEdge(node, menu);
            if (le < minLeft) minLeft = le;
        }

        if (minLeft == FLT_MAX) {
            btn->setPosition({ 0.f, baseY });
            btn->setZOrder(60);
            return;
        }

        // ---- TWEAKS HERE ----
        constexpr float padding = 6.f;
        constexpr float yOffset = -2.f; // brings it DOWN a bit (more negative = lower)
        // ---------------------

        btn->setPositionY(baseY + yOffset);

        // Compute translate button's right offset using visual bounds (so 0 contentSize isn't a problem)
        btn->setPositionX(0.f);
        float btnRight = visualRightEdge(btn, menu);
        float rightOffset = btnRight - btn->getPositionX();

        float desiredRightEdge = minLeft - padding;
        float newX = desiredRightEdge - rightOffset;

        btn->setPosition({ newX, baseY + yOffset });
        btn->setZOrder(60);
    }

    void applyCachedTranslation() {
        if (!m_comment || m_comment->m_commentDeleted) return;
        auto it = s_cache.find(m_comment->m_commentID);
        if (it == s_cache.end()) return;
        this->applyText(it->second);
    }

    void applyText(std::string const& text) {
        if (m_comment) m_comment->m_commentString = text;
        if (!m_mainLayer) return;

        if (auto label = typeinfo_cast<CCLabelBMFont*>(
            m_mainLayer->getChildByIDRecursive("comment-text-label")
        )) {
            label->setString(text.c_str());
        }

        if (auto area = typeinfo_cast<TextArea*>(
            m_mainLayer->getChildByIDRecursive("comment-text-area")
        )) {
            area->setString(text);
        }
    }

    void setButtonBusy(bool busy) {
        if (!m_mainLayer) return;

        auto menu = typeinfo_cast<CCMenu*>(m_mainLayer->getChildByIDRecursive("main-menu"));
        if (!menu) return;

        auto btn = typeinfo_cast<CCMenuItemSpriteExtra*>(
            menu->getChildByIDRecursive(makeNodeID("translate-button"))
        );
        if (!btn) return;

        btn->setEnabled(!busy);
        btn->setOpacity(busy ? 120 : 255);
    }

    void onTranslate(CCObject*) {
        if (!m_comment || m_comment->m_commentDeleted) return;

        auto id = m_comment->m_commentID;
        if (auto it = s_cache.find(id); it != s_cache.end()) {
            this->applyText(it->second);
            return;
        }

        auto original = std::string(m_comment->m_commentString);
        if (original.empty()) return;

        this->setButtonBusy(true);

        auto req = web::WebRequest();
        req.param("client", "gtx");
        req.param("sl", "auto");
        req.param("tl", "en");
        req.param("dt", "t");
        req.param("q", original);
        req.timeout(std::chrono::seconds(12));

        m_fields->m_translateTask.spawn(
            req.get("https://translate.googleapis.com/translate_a/single"),
            [this, id](web::WebResponse res) {
                this->setButtonBusy(false);
                if (!res.ok()) return;

                auto jsonRes = res.json();
                if (!jsonRes) return;

                auto translated = parseTranslation(jsonRes.unwrap());
                if (translated.empty()) return;

                s_cache[id] = translated;
                if (m_comment && m_comment->m_commentID == id) {
                    this->applyText(translated);
                }
            }
        );
    }
};