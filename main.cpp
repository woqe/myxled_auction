#include <bot.h>
#include "Item.h"
#include <memory>

#define TELEGRAM_TOKEN "1316750014:AAE00WkHBaJb3SfhpQTmHzKPICcnfGrDtY8"
#define CHAT_ID -1001431080993
#define ADMIN_USERNAME "AVGmsk"
#define ADMIN_CHAT_ID 214430860
#define AUCTION_TIME_MIN 30

void delete_messages(TelegramBot& bot, long long telegram_chat_id, DBMessage& db_message, Message::MESSAGE_TYPE type = Message::MESSAGE_TYPE::NULL_OPERATION){
    vector<Message>* messages = db_message.get_elems();
    for(auto it = messages->begin(); it != messages->end(); ++it){
        Message& msg = *it;
        if(!msg.to_delete && telegram_chat_id == msg.telegram_chat_id && (type == msg.operation || type == Message::MESSAGE_TYPE::NULL_OPERATION)){
            msg.mark_delete();
            bot.deleteMessage(telegram_chat_id, msg.message_id);
        }
    }
}

int send_bet_notify(TelegramBot& bot, long long telegram_chat_id, const Auction& lot, DBMessage& db_message, const Telegram::Update& update){
    json result = bot.sendPhoto(to_string(telegram_chat_id), lot.photo_1);
    db_message.add(Message(telegram_chat_id, int(result["result"]["message_id"])));
    if(lot.photo_2.size()){
        result = bot.sendPhoto(to_string(telegram_chat_id), lot.photo_2);
        db_message.add(Message(telegram_chat_id, int(result["result"]["message_id"])));
        if(lot.photo_3.size()){
            result = bot.sendPhoto(to_string(telegram_chat_id), lot.photo_3);
            db_message.add(Message(telegram_chat_id, int(result["result"]["message_id"])));
        }
    }
    vector<vector<Telegram::InlineKeyboardButton>> btns(0);
    for(int i = 5; i > 0; --i){
        int diff = lot.max_step - lot.min_step;
        int curr_bet = lot.price + lot.min_step + (lot.max_step - lot.min_step) / i;
        btns.push_back({Telegram::InlineKeyboardButton(to_string(curr_bet) + "₽", to_string(
                update.get_callback_encoded(
                        Telegram::Update::CALLBACK_TYPE::NEW_BET,
                        false, curr_bet)))});
    }
    result = bot.sendInlineKeyboard(to_string(telegram_chat_id), "Разыгрывается лот *" + markdown_free(lot.name) + "*\nОписание:\n\n*" + markdown_free(lot.desc) + "*\n\nАукцион закроется в *" + get_formatted_time(lot.end_date) + "* по МСК \\(Через " + markdown_free(to_string((lot.end_date - get_msk_time()) / 60 % 60)) + " мин\\.\\), последняя ставка\\: *" + to_string(lot.price) + "₽*\nПредлагайте ваши ставки:", btns);
    bot.pinChatMessage(telegram_chat_id, int(result["result"]["message_id"]));
    db_message.add(Message(telegram_chat_id, int(result["result"]["message_id"])));
}

int send_edit_menu(TelegramBot& bot, DBAuction& db_auction, User& user, Telegram::Update& update){
    Auction* auc = db_auction.get_edit();
    if(auc == nullptr)
        auc = db_auction.add(Auction());
    Auction& lot = *auc;
    string time = "Не установлено";
    if(lot.start_date)
        time = get_formatted_time(lot.start_date);
    string photo = "Не установлено";
    if(lot.photo_1.size())
        photo = "Установлено";

    return user.user_send_keyboard(bot, "Редактрование нового лота", {{Telegram::InlineKeyboardButton("Имя: " + markdown_free(lot.name), to_string(
            update.get_callback_encoded(
                    Telegram::Update::CALLBACK_TYPE::EDIT_NAME,
                    true)))},
                                                                        {Telegram::InlineKeyboardButton("Описание: " + markdown_free(lot.desc), to_string(
                                                                                update.get_callback_encoded(
                                                                                        Telegram::Update::CALLBACK_TYPE::EDIT_DESC,
                                                                                        true)))},
                                                                        {Telegram::InlineKeyboardButton("Стартовая цена: " + to_string(lot.price) + "₽", to_string(
                                                                                update.get_callback_encoded(
                                                                                        Telegram::Update::CALLBACK_TYPE::EDIT_PRICE,
                                                                                        true)))},
                                                                        {Telegram::InlineKeyboardButton("Минимальный шаг: " + to_string(lot.min_step) + "₽", to_string(
                                                                                update.get_callback_encoded(
                                                                                        Telegram::Update::CALLBACK_TYPE::EDIT_MIN_BET,
                                                                                        true)))},
                                                                        {Telegram::InlineKeyboardButton("Максимальный шаг: " + to_string(lot.max_step) + "₽", to_string(
                                                                                update.get_callback_encoded(
                                                                                        Telegram::Update::CALLBACK_TYPE::EDIT_MAX_BET,
                                                                                        true)))},
                                                                        {Telegram::InlineKeyboardButton("Фото: " + photo, to_string(
                                                                                update.get_callback_encoded(
                                                                                        Telegram::Update::CALLBACK_TYPE::EDIT_PHOTO,
                                                                                        true)))},
                                                                      {Telegram::InlineKeyboardButton("Время: " + time, to_string(
                                                                              update.get_callback_encoded(
                                                                                      Telegram::Update::CALLBACK_TYPE::EDIT_TIME,
                                                                                      true)))},
                                                                        {Telegram::InlineKeyboardButton("Отправить", to_string(
                                                                                update.get_callback_encoded(
                                                                                        Telegram::Update::CALLBACK_TYPE::SEND_LOT,
                                                                                        true)))}});
}

int main(){
    DBMessage db_message;
    DBAuction db_auction;

    while(1){
        TelegramBot bot(TELEGRAM_TOKEN);
        Auction* auc = db_auction.get_wait();
        if(auc != nullptr){
            Auction& lot = *auc;
            if(get_msk_time() >= lot.start_date){
                lot.status = Auction::STATUS::ACTIVE;
                send_bet_notify(bot, CHAT_ID, lot, db_message, Telegram::Update());
            }
        }

        auc = db_auction.get_active();
        if(auc != nullptr){
            Auction& lot = *auc;
            if(get_msk_time() >= lot.end_date){
                lot.status = Auction::STATUS::CLOSED;
                bot.sendMessage(to_string(CHAT_ID), "Аукцион по лоту " + markdown_free(lot.name) + " закрыт\\. Победитель @" + markdown_free(lot.winner_1) + " забрал лот за *" + to_string(lot.price) + "₽*\n\nПобедитель, свяжись с @hempled чтобы забрать свой лот");
                bot.sendMessage(to_string(ADMIN_CHAT_ID), "Аукцион по лоту " + markdown_free(lot.name) + " закрыт \\(id: " + to_string(lot.id) + "\\)\\\n1 место: @" + markdown_free(lot.winner_1) + ", стоимость: " + to_string(lot.winner_1_summ) + "\n" + "2 место: @" + markdown_free(lot.winner_2) + ", стоимость: " + to_string(lot.winner_2_summ) + "\n"+ "3 место: @" + markdown_free(lot.winner_3) + ", стоимость: " + to_string(lot.winner_3_summ));
                delete_messages(bot, CHAT_ID, db_message);
            }
        }

        auto_ptr<vector<Telegram::Update>> pUpdates(bot.getUpdates(10));
        vector<Telegram::Update>& updates = *pUpdates;

        if(!updates.size()){
            db_message.save_all();
            db_auction.save_all();
            continue;
        }

        for(auto it = updates.begin(); it != updates.end(); ++it){
            Telegram::Update& update = *it;
            User user(update);
            if(update.get_message() && update.get_message().get_text().size()) { //text message
                string text = update.get_message().get_text();

                if(update.get_message().get_chat().get_id() == update.get_message().get_from().get_id() && update.get_message().get_from().get_username() == ADMIN_USERNAME){

                    if(db_message.get_last_service_message() != nullptr){
                        Message::MESSAGE_TYPE operation = db_message.get_last_service_message()->operation;
                        vector<Message>& messages = *db_message.get_elems();
                        for(auto it = messages.begin(); it != messages.end(); ++it){
                            Message& msg = *it;
                            if(!msg.to_delete && msg.telegram_chat_id == user.get_chat_telegram_id()){
                                msg.mark_delete();
                                bot.deleteMessage(msg.telegram_chat_id, msg.message_id);
                            }
                        }
                        bot.deleteMessage(user.get_chat_telegram_id(), update.get_message().get_id());

                        Auction* auc = db_auction.get_edit();
                        if(auc == nullptr)
                            auc = db_auction.add(Auction());
                        Auction& lot = *auc;

                        switch(operation){
                            case Message::MESSAGE_TYPE::NAME:{
                                lot.name = text;
                                int message = send_edit_menu(bot, db_auction, user, update);
                                db_message.add(Message(user.get_chat_telegram_id(), message));
                                break;
                            }
                            case Message::MESSAGE_TYPE::DESC:{
                                lot.desc = text;
                                int message = send_edit_menu(bot, db_auction, user, update);
                                db_message.add(Message(user.get_chat_telegram_id(), message));
                                break;
                            }
                            case Message::MESSAGE_TYPE::PRICE:{
                                lot.price = atoi(text.c_str());
                                int message = send_edit_menu(bot, db_auction, user, update);
                                db_message.add(Message(user.get_chat_telegram_id(), message));
                                break;
                            }
                            case Message::MESSAGE_TYPE::MIN_BET:{
                                int num = atoi(text.c_str());
                                if(num >= lot.max_step){
                                    int message = user.user_send_keyboard(bot, "Минимальный шаг не может быть \\> или \\= максимальному шагу\\. Повторите операцию", {{Telegram::InlineKeyboardButton("Отменить операцию", to_string(update.get_callback_encoded(Telegram::Update::CALLBACK_TYPE::EDIT_MENU, true)))}});
                                    db_message.add(Message(user.get_chat_telegram_id(), message, Message::MESSAGE_TYPE::MIN_BET));
                                    break;
                                }
                                lot.min_step = atoi(text.c_str());
                                int message = send_edit_menu(bot, db_auction, user, update);
                                db_message.add(Message(user.get_chat_telegram_id(), message));
                                break;
                            }
                            case Message::MESSAGE_TYPE::MAX_BET:{
                                int num = atoi(text.c_str());
                                if(num <= lot.min_step){
                                    int message = user.user_send_keyboard(bot, "Максимальный шаг не может быть \\< или \\= минимальному шагу\\. Повторите операцию", {{Telegram::InlineKeyboardButton("Отменить операцию", to_string(update.get_callback_encoded(Telegram::Update::CALLBACK_TYPE::EDIT_MENU, true)))}});
                                    db_message.add(Message(user.get_chat_telegram_id(), message, Message::MESSAGE_TYPE::MAX_BET));
                                    break;
                                }
                                lot.max_step = atoi(text.c_str());
                                int message = send_edit_menu(bot, db_auction, user, update);
                                db_message.add(Message(user.get_chat_telegram_id(), message));
                                break;
                            }
                        }
                    }else{
                        if(text == "бот" || text == "Бот"){
                            Auction* auc = db_auction.get_edit();
                            if(auc == nullptr)
                                auc = db_auction.add(Auction());
                            Auction& lot = *auc;
                            int message = send_edit_menu(bot, db_auction, user, update);
                            db_message.add(Message(user.get_chat_telegram_id(), message));
                        }
                        if(text == "тест" || text == "Тест"){
                            Auction* auc = db_auction.get_edit();
                            if(auc == nullptr)
                                auc = db_auction.add(Auction());
                            Auction& lot = *auc;
                            int message = send_bet_notify(bot, user.get_chat_telegram_id(), lot, db_message, update);
                            db_message.add(Message(user.get_chat_telegram_id(), message));
                        }
                    }
                }
                else if(update.get_message().get_chat().get_id() == CHAT_ID){

                }
            }
            else if(update.get_message() && update.get_message().get_photo()){
                if(db_message.get_last_service_message() != nullptr) {
                    Message::MESSAGE_TYPE operation = db_message.get_last_service_message()->operation;
                    vector<Message> &messages = *db_message.get_elems();
                    for (auto it = messages.begin(); it != messages.end(); ++it) {
                        Message &msg = *it;
                        if (!msg.to_delete && msg.telegram_chat_id == user.get_chat_telegram_id()) {
                            msg.mark_delete();
                            bot.deleteMessage(msg.telegram_chat_id, msg.message_id);
                        }
                    }
                    bot.deleteMessage(user.get_chat_telegram_id(), update.get_message().get_id());

                    Auction *auc = db_auction.get_edit();
                    if (auc == nullptr)
                        auc = db_auction.add(Auction());
                    Auction &lot = *auc;

                    switch (operation) {
                        case Message::MESSAGE_TYPE::PHOTO:{
                            lot.photo_1 = update.get_message().get_photo().get_file_id();
                            if((it + 1) != updates.end() && (it + 1)->get_message().get_photo()){
                                lot.photo_2 = (it + 1)->get_message().get_photo().get_file_id();
                                bot.deleteMessage(user.get_chat_telegram_id(), (it + 1)->get_message().get_id());
                                if((it + 2) != updates.end() && (it + 2)->get_message().get_photo()) {
                                    lot.photo_3 = (it + 2)->get_message().get_photo().get_file_id();
                                    bot.deleteMessage(user.get_chat_telegram_id(), (it + 2)->get_message().get_id());
                                }
                            }
                            int message = send_edit_menu(bot, db_auction, user, update);
                            db_message.add(Message(user.get_chat_telegram_id(), message));
                            break;
                        }
                    }
                }
            }
            else if(update.get_callback_query()){

                if(update.get_callback_is_delete()){
                    vector<Message>& messages = *db_message.get_elems();
                    for(auto it = messages.begin(); it != messages.end(); ++it){
                        Message& msg = *it;
                        if(!msg.to_delete && msg.telegram_chat_id == user.get_chat_telegram_id()){
                            msg.mark_delete();
                            bot.deleteMessage(msg.telegram_chat_id, msg.message_id);
                        }
                    }
                }

                if(update.get_callback_query().get_message().get_chat().get_id() == update.get_callback_query().get_from().get_id() && update.get_callback_query().get_from().get_username() == ADMIN_USERNAME){
                    Auction* auc = db_auction.get_edit();
                    if(auc == nullptr)
                        auc = db_auction.add(Auction());
                    Auction& lot = *auc;

                    switch(update.get_callback_type()) {
                        case Telegram::Update::CALLBACK_TYPE::EDIT_TIME_CHOOSE:{
                            long long epoch = update.get_callback_data();
                            lot.start_date = epoch;
                            lot.end_date = epoch + AUCTION_TIME_MIN * 60;
                            int message = send_edit_menu(bot, db_auction, user, update);
                            db_message.add(Message(user.get_chat_telegram_id(), message));
                            break;
                        }
                        case Telegram::Update::CALLBACK_TYPE::EDIT_TIME:{
                            vector<vector<Telegram::InlineKeyboardButton>> btns(0);
                            btns.push_back({Telegram::InlineKeyboardButton("Сейчас", to_string(update.get_callback_encoded(Telegram::Update::CALLBACK_TYPE::EDIT_TIME_CHOOSE, true, get_msk_time())))});
                            long long current_epoch = get_msk_time();
                            int current_minutes = current_epoch / 60 % 60;
                            long long next_hour_epoch =  current_epoch - (current_minutes * 60) + (60 * 60);
                            for(int i = 0; i < (24 - (next_hour_epoch / 60 / 60 % 24)); ++i)
                                btns.push_back({Telegram::InlineKeyboardButton(get_formatted_time(next_hour_epoch + i * (60 * 60)), to_string(update.get_callback_encoded(Telegram::Update::CALLBACK_TYPE::EDIT_TIME_CHOOSE, true, next_hour_epoch + i * (60 * 60))))});
                            btns.push_back({Telegram::InlineKeyboardButton("Отменить операцию", to_string(update.get_callback_encoded(Telegram::Update::CALLBACK_TYPE::EDIT_MENU, true)))});
                            int message = user.user_send_keyboard(bot, "Выберите время на сегодня", btns);
                            db_message.add(Message(user.get_chat_telegram_id(), message));
                            break;
                        }
                        case Telegram::Update::CALLBACK_TYPE::EDIT_PHOTO:{
                            int message = user.user_send_keyboard(bot, "Отправьте от 1 до 3 фото", {{Telegram::InlineKeyboardButton("Отменить операцию", to_string(update.get_callback_encoded(Telegram::Update::CALLBACK_TYPE::EDIT_MENU, true)))}});
                            db_message.add(Message(user.get_chat_telegram_id(), message, Message::MESSAGE_TYPE::PHOTO));
                            break;
                        }
                        case Telegram::Update::CALLBACK_TYPE::EDIT_MAX_BET:{
                            int message = user.user_send_keyboard(bot, "Введите максимальный шаг ставки\\. Текущий максимальный шаг: " + to_string(lot.max_step), {{Telegram::InlineKeyboardButton("Отменить операцию", to_string(update.get_callback_encoded(Telegram::Update::CALLBACK_TYPE::EDIT_MENU, true)))}});
                            db_message.add(Message(user.get_chat_telegram_id(), message, Message::MESSAGE_TYPE::MAX_BET));
                            break;
                        }
                        case Telegram::Update::CALLBACK_TYPE::EDIT_MIN_BET:{
                            int message = user.user_send_keyboard(bot, "Введите минимальный шаг ставки\\. Текущий минимальный шаг: " + to_string(lot.min_step), {{Telegram::InlineKeyboardButton("Отменить операцию", to_string(update.get_callback_encoded(Telegram::Update::CALLBACK_TYPE::EDIT_MENU, true)))}});
                            db_message.add(Message(user.get_chat_telegram_id(), message, Message::MESSAGE_TYPE::MIN_BET));
                            break;
                        }
                        case Telegram::Update::CALLBACK_TYPE::EDIT_PRICE:{
                            int message = user.user_send_keyboard(bot, "Введите начальную цену лота\\. Старая цена: " + to_string(lot.price), {{Telegram::InlineKeyboardButton("Отменить операцию", to_string(update.get_callback_encoded(Telegram::Update::CALLBACK_TYPE::EDIT_MENU, true)))}});
                            db_message.add(Message(user.get_chat_telegram_id(), message, Message::MESSAGE_TYPE::PRICE));
                            break;
                        }
                        case Telegram::Update::CALLBACK_TYPE::EDIT_DESC:{
                            int message = user.user_send_keyboard(bot, "Введите описание лота\\. Старое описание: " + markdown_free(lot.desc), {{Telegram::InlineKeyboardButton("Отменить операцию", to_string(update.get_callback_encoded(Telegram::Update::CALLBACK_TYPE::EDIT_MENU, true)))}});
                            db_message.add(Message(user.get_chat_telegram_id(), message, Message::MESSAGE_TYPE::DESC));
                            break;
                        }
                        case Telegram::Update::CALLBACK_TYPE::EDIT_NAME:{
                            int message = user.user_send_keyboard(bot, "Введите название лота\\. Старое название: " + markdown_free(lot.name), {{Telegram::InlineKeyboardButton("Отменить операцию", to_string(update.get_callback_encoded(Telegram::Update::CALLBACK_TYPE::EDIT_MENU, true)))}});
                            db_message.add(Message(user.get_chat_telegram_id(), message, Message::MESSAGE_TYPE::NAME));
                            break;
                        }
                        case Telegram::Update::CALLBACK_TYPE::SEND_LOT:{
                            int message;
                            if(lot.name == "" || lot.desc == "" || lot.photo_1 == "" || lot.start_date == 0){
                                message = user.user_sendMessage(bot, "Имя, описание, фото или время не установлено\\. Это обязательные поля");
                            }else{
                                lot.status = Auction::STATUS::WAITING;
                                message = user.user_sendMessage(bot, "Лот отправлен в очередь");
                            }
                            db_message.add(Message(user.get_chat_telegram_id(), message));
                            message = send_edit_menu(bot, db_auction, user, update);
                            db_message.add(Message(user.get_chat_telegram_id(), message));

                            break;
                        }
                        case Telegram::Update::CALLBACK_TYPE::EDIT_MENU:{
                            int message = send_edit_menu(bot, db_auction, user, update);
                            db_message.add(Message(user.get_chat_telegram_id(), message));
                            break;
                        }
                    }
                }
                else if(update.get_callback_query().get_message().get_chat().get_id() == CHAT_ID){

                    Auction* auc = db_auction.get_active();
                    if(auc == nullptr)
                        continue;
                    Auction& lot = *auc;

                    switch(update.get_callback_type()) {
                        case Telegram::Update::CALLBACK_TYPE::NEW_BET:{
                            if(!user.get_username().size()){
                                delete_messages(bot, user.get_chat_telegram_id(), db_message, Message::MESSAGE_TYPE::UNDEFINED_USERNAME);
                                int message = user.chat_sendMessage(bot, "*" + markdown_free(update.get_callback_query().get_from().get_first_name()) + "*, установи юзернейм \\(ник пользователя\\) в настройках аккаунта, прежде чем участвовать в аукционе");
                                db_message.add(Message(user.get_chat_telegram_id(), message, Message::MESSAGE_TYPE::UNDEFINED_USERNAME));
                                break;
                            }
                            int bet = update.get_callback_data();
                            if(bet > lot.price && bet - lot.price >= lot.min_step && bet - lot.price <= lot.max_step && user.get_username() != lot.winner_1){
                                lot.winner_3 = lot.winner_2;
                                lot.winner_3_summ = lot.winner_2_summ;
                                lot.winner_2 = lot.winner_1;
                                lot.winner_2_summ = lot.winner_1_summ;
                                lot.winner_1 = user.get_username();
                                lot.winner_1_summ = bet;
                                lot.price = bet;
                                delete_messages(bot, user.get_chat_telegram_id(), db_message);
                                user.chat_sendMessage(bot, "*@" + markdown_free(lot.winner_1) + "* сделал ставку *" + to_string(bet) + "₽*\nКто даст больше?");
                                int message = send_bet_notify(bot, user.get_chat_telegram_id(), lot, db_message, update);
                                db_message.add(Message(user.get_chat_telegram_id(), message));
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
}

class DataContainerInterface{
private:

public:
    virtual void put_file(fstream& stream) = 0;
};

class SampleContainer : public DataContainerInterface{
private:
    struct Settings {
        int year;
        char name[52];
    } settings;
public:
    void put_file(fstream& stream){
        stream << settings.year << settings.name;
    }
};


