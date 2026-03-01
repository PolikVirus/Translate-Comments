#include <Geode/Geode.hpp>
#include <Geode/modify/CommentCell.hpp>
#include <Geode/modify/IDManager.hpp>
#include <Geode/binding/GJComment.hpp>
#include <Geode/binding/TextArea.hpp>
#include <Geode/utils/web.hpp>
#include <matjson.hpp>
#include <chrono>
#include <unordered_map>

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
}

class $modify(TranslatingCommentCell, CommentCell) {
    struct Fields {
        TaskHolder<web::WebResponse> m_translateTask;
    };
    void loadFromComment(GJComment * comment) {
        CommentCell::loadFromComment(comment);
        NodeIDs::provideFor(this);

        this->installTranslateButton();
        this->applyCachedTranslation();
    }

    void installTranslateButton() {
        if (!m_mainLayer || !m_comment || m_comment->m_commentDeleted) return;

        auto menu = typeinfo_cast<CCMenu*>(m_mainLayer->getChildByIDRecursive("main-menu"));
        if (!menu) return;

        auto btnID = makeNodeID("translate-button");
        if (auto old = menu->getChildByID(btnID)) old->removeFromParent();

        auto icon = CCSprite::create("translate.png"_spr);
        if (!icon) return;
        icon->setScale(0.65f);

        auto btn = CCMenuItemSpriteExtra::create(
            icon, this, menu_selector(TranslatingCommentCell::onTranslate)
        );
        btn->setID(btnID);

        menu->addChild(btn);
        this->positionButtonUnderLike(menu, btn);
    }

    void positionButtonUnderLike(CCMenu * menu, CCNode * btn) {
        if (!menu || !btn) return;
        CCNode* like =
            (menu->getChildByID("like-button") ? menu->getChildByID("like-button") :
                (menu->getChildByID("spam-button") ? menu->getChildByID("spam-button") :
                    (menu->getChildByID("delete-button") ? menu->getChildByID("delete-button") : nullptr)));
        if (!like) {
            btn->setPosition(ccp(0.f, 0.f));
            btn->setZOrder(50);
            return;
        }
        // offset
        constexpr float offsetX = 27.f;
        constexpr float offsetY = 26.f;

        btn->setPosition(like->getPosition() + ccp(offsetX, -offsetY));
        btn->setZOrder(50);
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
            menu->getChildByID(makeNodeID("translate-button"))
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