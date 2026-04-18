#include <Geode/Geode.hpp>
#include <Geode/Bindings.hpp>
#include <Geode/modify/CommentCell.hpp>
#include <Geode/utils/web.hpp>
#include <limits>
using namespace geode::prelude;

/*
    just so you know
    diacritic removal is basically removing accent marks like Ä and turning them to A
*/
std::string diacritic_removal(std::string text) {
    std::vector<std::pair<std::string, std::string>> repl = {
        // a
        {"à", "a"}, {"á", "a"}, {"â", "a"}, {"ã", "a"}, {"ä", "a"}, {"å", "a"},
        {"ā", "a"}, {"ą", "a"},
        {"À", "A"}, {"Á", "A"}, {"Â", "A"}, {"Ã", "A"}, {"Ä", "A"}, {"Å", "A"},
        {"Ā", "A"}, {"Ą", "A"},

        // c
        {"ç", "c"}, {"č", "c"}, {"ć", "c"},
        {"Ç", "C"}, {"Č", "C"}, {"Ć", "C"},

        // e
        {"è", "e"}, {"é", "e"}, {"ê", "e"}, {"ë", "e"},
        {"ē", "e"}, {"ę", "e"}, {"ė", "e"},
        {"È", "E"}, {"É", "E"}, {"Ê", "E"}, {"Ë", "E"},
        {"Ē", "E"}, {"Ę", "E"}, {"Ė", "E"},

        // g
        {"ģ", "g"}, {"ğ", "g"},
        {"Ģ", "G"}, {"Ğ", "G"},

        // i
        {"ì", "i"}, {"í", "i"}, {"î", "i"}, {"ï", "i"},
        {"ī", "i"}, {"į", "i"}, {"ı", "i"},
        {"Ì", "I"}, {"Í", "I"}, {"Î", "I"}, {"Ï", "I"},
        {"Ī", "I"}, {"Į", "I"}, {"İ", "I"},

        // k
        {"ķ", "k"},
        {"Ķ", "K"},

        // l
        {"ļ", "l"}, {"ł", "l"},
        {"Ļ", "L"}, {"Ł", "L"},

        // n
        {"ñ", "n"}, {"ņ", "n"}, {"ń", "n"},
        {"Ñ", "N"}, {"Ņ", "N"}, {"Ń", "N"},

        // o
        {"ò", "o"}, {"ó", "o"}, {"ô", "o"}, {"õ", "o"}, {"ö", "o"}, {"ø", "o"},
        {"ō", "o"}, {"õ", "o"}, {"ó", "o"},
        {"Ò", "O"}, {"Ó", "O"}, {"Ô", "O"}, {"Õ", "O"}, {"Ö", "O"}, {"Ø", "O"},
        {"Ō", "O"},

        // s
        {"ß", "ss"}, {"š", "s"}, {"ś", "s"},
        {"Š", "S"}, {"Ś", "S"},

        // u
        {"ù", "u"}, {"ú", "u"}, {"û", "u"}, {"ü", "u"},
        {"ū", "u"}, {"ų", "u"},
        {"Ù", "U"}, {"Ú", "U"}, {"Û", "U"}, {"Ü", "U"},
        {"Ū", "U"}, {"Ų", "U"},

        // y
        {"ÿ", "y"},
        {"Ÿ", "Y"},

        // z
        {"ž", "z"}, {"ź", "z"}, {"ż", "z"},
        {"Ž", "Z"}, {"Ź", "Z"}, {"Ż", "Z"}
    };

    // replace
    for (auto const& [from, to] : repl) {
        size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            text.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    return text;
}


class $modify(Translatehook, CommentCell) {
    struct Fields {
        async::TaskHolder<web::WebResponse> m_listener;
        GJComment* m_comment = nullptr;
    };
    
    void loadFromComment(GJComment* comment) {
        
        // get main-menu where the button will be at
        CommentCell::loadFromComment(comment);
        m_fields->m_comment = comment;
        auto node = m_mainLayer->getChildByID("main-menu");
        if (!node) return;

        auto menu = typeinfo_cast<CCMenu*>(node);
        if (!menu) return;
        if (menu->getChildByID("translate-button"_spr)) return;
        

    
        auto btnsprite = CCSprite::create("translate.png"_spr);
        auto translatebtn = CCMenuItemSpriteExtra::create(
            btnsprite,
            this,
            menu_selector(Translatehook::onTranslate)
        );

        translatebtn->setID("translate-button"_spr);


        // this is better that how i did it before, but it still causes overlapping with some mods
        CCMenuItemSpriteExtra* leftmostButton = nullptr;
        float leftmostX = std::numeric_limits<float>::max();

        for (int i = 0; i < menu->getChildrenCount(); i++) {
            auto child = static_cast<CCNode*>(menu->getChildren()->objectAtIndex(i));
            if (!child) continue;

            auto button = typeinfo_cast<CCMenuItemSpriteExtra*>(child);
            if (!button) continue;

            float x = button->getPositionX();
            if (x < leftmostX) {
                leftmostX = x;
                leftmostButton = button;
            }
        }

        if (leftmostButton) {
            auto pos = leftmostButton->getPosition();
            translatebtn->setPosition({ pos.x - 30.f, pos.y });
            btnsprite->setScale(0.8f);
        }
        
        menu->addChild(translatebtn);

    }
    // translate btn func
    void onTranslate(CCObject* sender) {
        if (!m_fields->m_comment) return;

        std::string ctext = m_fields->m_comment->m_commentString;
        web::WebRequest req;
        
        
        auto targetlang = Mod::get()->getSettingValue<std::string>("target-language");
        std::string language;


        // there is 100% a better way to do this, but it works
        if (targetlang == "English") {
            language = std::string("en");
        }
        else if (targetlang == "Spanish") {
            language = std::string("es");
        }
        else if (targetlang == "German") {
            language = std::string("de");
        }
        else if (targetlang == "French") {
            language = std::string("fr");
        }
        else if (targetlang == "Swedish") {
            language = std::string("sv");
        }
        else if (targetlang == "Latvian") {
            language = std::string("lv");
        }
        else if (targetlang == "Polish") {
            language = std::string("pl");
        }
        else if (targetlang == "Lithuanian") {
            language = std::string("lt");
        }
        else if (targetlang == "Estonian") {
            language = std::string("et");
        }
        else if (targetlang == "Portuguese") {
            language = std::string("pt");
        }
        else if (targetlang == "Italian") {
            language = std::string("it");
        }
        else if (targetlang == "Dutch") {
            language = std::string("nl");
        }
        else if (targetlang == "Turkish") {
            language = std::string("tr");
        }
        else if (targetlang == "Romanian") {
            language = std::string("ro");
        }
        else if (targetlang == "Finnish") {
            language = std::string("fi");
        }
        else if (targetlang == "Danish") {
            language = std::string("da");
        }
        else if (targetlang == "Norwegian") {
            language = std::string("no");
        }
        else {
            language = "en";
        }

        // parameters
        req.userAgent("Translate Comments");
        req.param("client", "gtx");
        req.param("sl", "auto"); // detect lang
        req.param("tl", language);  // target lang
        req.param("dt", "t");
        req.param("q", ctext);

        m_fields->m_listener.spawn(
            req.get("https://translate.googleapis.com/translate_a/single"),
            [this](web::WebResponse res) {

                // error handlers
                
                // this makes an alert for each error for the user
                // i included only the common ones
                if (res.code() != 200) {
                    switch (res.code()) {
                        case -7:
                            FLAlertLayer::create(
                                "Translate comments",
                                "Error: Couldn't connect to server",
                                "Ok"
                            )->show();
                            break;

                        case -28:
                            FLAlertLayer::create(
                                "Translate comments",
                                "Error: Timed out",
                                "Ok"
                            )->show();
                            break;

                        case -6:
                            FLAlertLayer::create(
                                "Translate comments",
                                "Error: Couldn't resolve host",
                                "Ok"
                            )->show();
                            break;

                        case 400:
                            FLAlertLayer::create(
                                "Translate comments",
                                "Error: Bad request",
                                "Ok"
                            )->show();
                            break;

                        case 401:
                            FLAlertLayer::create(
                                "Translate comments",
                                "Error: Unauthorized",
                                "Ok"
                            )->show();
                            break;

                        case 429:
                            FLAlertLayer::create(
                                "Translate comments",
                                "Error: Rate limit",
                                "Ok"
                            )->show();
                            break;

                        case 500:
                            FLAlertLayer::create(
                                "Translate comments",
                                "Error: Server-side issues",
                                "Ok"
                            )->show();
                            break;
                        case 503:
                            FLAlertLayer::create(
                                "Translate comments",
                                "Error: Server-side issues",
                                "Ok"
                            )->show();
                            break;

                        default:
                            FLAlertLayer::create(
                                "Translate comments",
                                "Error: Request failed",
                                "Ok"
                            )->show();
                            break;
                    }
                    return;
                }
                // check translation body
                auto body = res.string().unwrapOr("Uh oh!");
                if (body.empty()) {
                    log::warn("translation response body was empty");
                    return;
                }

                // check parse
                auto parsed = matjson::parse(body);
                if (!parsed) {
                    log::info("failed to parse response");
                    return;
                }
                // check if array
                auto const& root = *parsed;
                if (!root.isArray()) {
                    log::warn("translation response root was not an array");
                    return;
                }
                auto sentences = root[0];
                if (!sentences.isArray() || sentences.size() == 0) {
                    log::warn("translation response missing sentence array");
                    return;
                }

                auto firstSentence = sentences[0];
                if (!firstSentence.isArray() || firstSentence.size() == 0) {
                    log::warn("translation response missing first sentence entry: {}", body);
                    return;
                }
                // check if string
                auto translation = firstSentence[0];
                if (!translation.isString()) {
                    log::warn("translation is not a string");
                    return;
                }

                auto translated = translation.asString().unwrapOr("");
                // log::info(translated);
                
                translated = diacritic_removal(translated);
                // set text
                if (translated.empty()) return;
                m_fields->m_comment->m_commentString = translated;

                auto textNode = typeinfo_cast<TextArea*>(m_mainLayer->getChildByID("comment-text-area"));
                if (textNode) {
                    textNode->setString(translated.c_str());
                    // log::info("area");
                }
                else {
                    auto textNodecompact = typeinfo_cast<CCLabelBMFont*>(m_mainLayer->getChildByID("comment-text-label"));
                    if (textNodecompact) {
                        textNodecompact->setString(translated.c_str());
                        // log::info("label");
                    }
                }
            }
        );

        // log::info("translate pressed");
    }
        
};