/*
 Copyright (c) 1996-2023 Freeciv21 and Freeciv contributors. This file is
 part of Freeciv21. Freeciv21 is free software: you can redistribute it
 and/or modify it under the terms of the GNU  General Public License  as
 published by the Free Software Foundation, either version 3 of the
 License,  or (at your option) any later version. You should have received
 a copy of the GNU General Public License along with Freeciv21. If not,
 see https://www.gnu.org/licenses/.
 */

/*
 * This file contains functions to generate the GUI for the research view
 * (formally known as the science dialog).
 */

// C
#include <ctime>

// Qt
#include <QComboBox>
#include <QGridLayout>
#include <QStackedLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QTimer>
#include <QToolTip>

// common
#include "chat.h"
#include "game.h"
// client
#include "client_main.h"
#include "climisc.h"
#include "helpdlg.h"
#include "text.h"
#include "tileset/sprite.h"
#include "tileset/tilespec.h"
// gui-qt
#include "citydlg.h"
#include "fc_client.h"
#include "page_game.h"
#include "tooltips.h"
#include "top_bar.h"
#include "views/view_government.h"
#include "views/view_sabotages.h"

std::string minutes_to_time_str(int minutes)
{
  int hours = minutes / 60;
  if(hours == 0) {
    return std::to_string(minutes) + "m";
  }
  minutes = minutes % 60;
  return std::to_string(hours) + "h" + std::to_string(minutes) + "m";
}

government_report* government_report::_instance = nullptr;

government_report *government_report::instance()
{
  if (!_instance) {
    _instance = new government_report();
  }
  return _instance;
}

audit_button::audit_button() : QPushButton()
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  set_audit_id(-1);
  connect(this, &QPushButton::clicked, this, [this]() {
    emit audit_selected(audit_id);
  });
}

int audit_button::get_audit_id() const { return audit_id; }

void audit_button::set_audit_id(int id)
{
  audit_id = id;
  if(audit_id == -1) {
    setText(_("No auditing occuring\nin this slot"));
    setEnabled(false);
  } else {
    setText(_("Loading..."));
    setEnabled(true);

    struct packet_government_audit_info_req *req =
        new packet_government_audit_info_req();

    req->id = audit_id;
    send_packet_government_audit_info_req(&client.conn, req);
  }
}

void audit_button::set_audit_info(struct government_audit_info *info)
{
  std::string accuser_name = player_id_to_string((player_id)info->accuser_id);
  std::string accused_name = player_id_to_string((player_id)info->accused_id);
  setText(QString("%1 vs %2")
    .arg(accuser_name.c_str())
    .arg(accused_name.c_str())
  );
}

/**
   Consctructor for government_report
 */
government_report::government_report() : QWidget()
{
  QSize size;
  QSizePolicy size_expand_policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  QSizePolicy size_expand_y_policy(QSizePolicy::Minimum, QSizePolicy::Expanding);
  QSizePolicy size_expand_x_policy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  QSizePolicy size_fixed_policy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  layout = new QStackedLayout();

  // Main screen
  QWidget* main_screen = new QWidget();
  QGridLayout* m_layout = new QGridLayout();
  QLabel* m_gov_label = new QLabel(_("Government"));
  m_gov_label->setSizePolicy(size_fixed_policy);
  m_gov_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_layout->addWidget(m_gov_label, 0, 0, -1, 4);

  QLabel* m_recent_decisions_label = new QLabel(_("Recent decisions:"));
  m_recent_decisions_label->setSizePolicy(size_fixed_policy);
  m_layout->addWidget(m_recent_decisions_label, 0, 4, 1, -1);

  m_recent_decisions_scroll = new QScrollArea();
  m_recent_decisions_scroll->setSizePolicy(size_expand_policy);
  QBoxLayout *m_recent_decisions_layout = new QBoxLayout(QBoxLayout::TopToBottom);
  m_recent_decisions_layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_recent_decisions_scroll->setLayout(m_recent_decisions_layout);
  m_layout->addWidget(m_recent_decisions_scroll, 1, 4, 7, -1);

  QLabel *m_auditing_label = new QLabel(_("Auditing:"));
  m_auditing_label->setSizePolicy(size_fixed_policy);
  m_layout->addWidget(m_auditing_label, 8, 4, 1, -1);

  QPushButton *m_auditing_start_button = new QPushButton();
  m_auditing_start_button->setSizePolicy(size_fixed_policy);
  m_auditing_start_button->setText(_("Make an accusation"));
  m_layout->addWidget(m_auditing_start_button, 9, 4, 1, -1, Qt::AlignLeft);
  QObject::connect(m_auditing_start_button, &QPushButton::clicked, this, &government_report::begin_audit);

  QWidget *m_auditing_widget = new QWidget();
  QHBoxLayout *m_auditing_layout = new QHBoxLayout();
  m_auditing_widget->setSizePolicy(size_expand_policy);
  m_auditing_widget->setLayout(m_auditing_layout);
  m_layout->addWidget(m_auditing_widget, 10, 4, 7, -1);

  m_auditing_buttons = new audit_button*[m_auditing_count];
  for(int i = 0; i < m_auditing_count; i++) {
    m_auditing_buttons[i] = new audit_button();
    m_auditing_layout->addWidget(m_auditing_buttons[i]);
    QObject::connect(m_auditing_buttons[i], &audit_button::audit_selected, this, &government_report::show_audit_screen);
    if(i != m_auditing_count - 1) {
      m_auditing_layout->addSpacing(45);
    }
  }

  main_screen->setSizePolicy(size_expand_policy);
  main_screen->setLayout(m_layout);

  // Auditing screen (accuser/accused)
  QWidget *auditing_screen = new QWidget();
  QGridLayout *a_layout = new QGridLayout();

  QPushButton *a_go_back = new QPushButton();
  a_go_back->setSizePolicy(size_fixed_policy);
  a_go_back->setText(_("Go back"));
  a_go_back->connect(a_go_back, &QPushButton::clicked, this, [this]() {
    layout->setCurrentIndex(0);
  });
  a_layout->addWidget(a_go_back, 0, 0, 1, 1);

  a_description = new QLabel(_("Description"));
  a_description->setSizePolicy(size_fixed_policy);
  a_layout->addWidget(a_description, 1, 0);//, -1, 1);

  a_player_description = new QLabel(_("Player description"));
  a_player_description->setWordWrap(true);
  a_player_description->setSizePolicy(size_expand_policy);
  a_layout->addWidget(a_player_description, 2, 0);//, -1, 1);

  a_accuser_pixmap_cont = new QLabel();
  a_accuser_pixmap_cont->setSizePolicy(size_fixed_policy);
  a_layout->addWidget(a_accuser_pixmap_cont, 3, 0);//, 3, 3);

  a_accused_pixmap_cont = new QLabel();
  a_accused_pixmap_cont->setSizePolicy(size_fixed_policy);
  a_layout->addWidget(a_accused_pixmap_cont, 3, 3);//, 3, 3);

  a_jury_1_pixmap_cont = new QLabel();
  a_jury_1_pixmap_cont->setSizePolicy(size_fixed_policy);
  a_layout->addWidget(a_jury_1_pixmap_cont, 7, 0);//, 3, 3);

  a_jury_2_pixmap_cont = new QLabel();
  a_jury_2_pixmap_cont->setSizePolicy(size_fixed_policy);
  a_layout->addWidget(a_jury_2_pixmap_cont, 7, 3);//, 3, 3);

  a_vote_confirm = new hud_message_box(this);
  a_vote_confirm->setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes);
  a_vote_confirm->setDefaultButton(QMessageBox::Cancel);
  // TODO: Connect confirm button to vote

  a_jury_vote_yes = new QPushButton();
  a_jury_vote_yes->setSizePolicy(size_fixed_policy);
  a_jury_vote_yes->setText(_("Vote \"true\""));
  connect(a_jury_vote_yes, &QPushButton::clicked, this, [this]() {
    confirm_vote(AUDIT_VOTE_YES);
  });
  a_layout->addWidget(a_jury_vote_yes, 10, 0, 1, 2);//, 1, 1);

  a_jury_vote_no = new QPushButton();
  a_jury_vote_no->setSizePolicy(size_fixed_policy);
  a_jury_vote_no->setText(_("Vote \"false\""));
  a_jury_vote_no->connect(a_jury_vote_no, &QPushButton::clicked, this, [this]() {
    confirm_vote(AUDIT_VOTE_NO);
  });
  a_layout->addWidget(a_jury_vote_no, 10, 2, 1, 2);//, 1, 1);

  a_jury_vote_abstain = new QPushButton();
  a_jury_vote_abstain->setSizePolicy(size_fixed_policy);
  a_jury_vote_abstain->setText(_("Abstain"));
  a_jury_vote_abstain->connect(a_jury_vote_abstain, &QPushButton::clicked, this, [this]() {
    confirm_vote(AUDIT_VOTE_ABSTAIN);
  });
  a_layout->addWidget(a_jury_vote_abstain, 10, 4, 1, 2);//, 1, 1);

  QLabel* a_public_chat_lbl = new QLabel(_("Public chat"));
  a_public_chat_lbl->setSizePolicy(size_expand_policy);
  a_layout->addWidget(a_public_chat_lbl, 0, 6);//, -1, 4);
  // TODO: Add public chat

  for(int i = 0; i < MAX_AUDIT_NUM; i++) {
    chat_widgets[i] = new chat_widget(this);
    chat_widgets[i]->setSizePolicy(size_expand_policy);
    chat_widgets[i]->setVisible(false);
    // Set a bogus filter, otherwise it will receive meta messages
    chat_widgets[i]->set_filter(QString(CHAT_AUDIT_PREFIX) + QString::number(i) + ":");
    a_layout->addWidget(chat_widgets[i], 1, 6, 5, 4);//, -1, 4);
  }

  QLabel* a_consequence_good_label = new QLabel(_("Consequence if true:"));
  a_consequence_good_label->setSizePolicy(size_expand_policy);
  a_layout->addWidget(a_consequence_good_label, 0, 11);//, 1, -1);

  a_consequence_good = new QLabel();
  a_consequence_good->setSizePolicy(size_expand_policy);
  a_layout->addWidget(a_consequence_good, 1, 11);//, 5, -1);

  QLabel* a_consequence_bad_label = new QLabel(_("Consequence if false:"));
  a_consequence_bad_label->setSizePolicy(size_expand_policy);
  a_layout->addWidget(a_consequence_bad_label, 6, 11);//, 1, -1);

  a_consequence_bad = new QLabel();
  a_consequence_bad->setSizePolicy(size_expand_policy);
  a_layout->addWidget(a_consequence_bad, 7, 11);//, 5, -1);

  auditing_screen->setSizePolicy(size_expand_policy);
  auditing_screen->setLayout(a_layout);

  // Add it all up
  layout->addWidget(main_screen);
  layout->addWidget(auditing_screen);
  setLayout(layout);
}

/**
   Destructor for government report
   Removes "GOV" string marking it as closed
   And frees given index on list marking it as ready for new widget
 */
government_report::~government_report()
{
  queen()->removeRepoDlg(QStringLiteral("GOV"));
}

/**
   Updates government_report and marks it as opened
   It has to be called soon after constructor.
   It could be in constructor but compiler will yell about not used variable
 */
void government_report::init(bool raise)
{
  Q_UNUSED(raise)
  queen()->gimmePlace(this, QStringLiteral("GOV"));
  index = queen()->addGameTab(this);
  queen()->game_tab_widget->setCurrentIndex(index);
}

void government_report::update_report()
{
  for(int i = 0; i < MAX_AUDIT_NUM; i++) {
    chat_widgets[i]->clearChat();
  }
}

/**
   Schedules paint event in some qt queue
 */
void government_report::redraw() { update(); }

void government_report::begin_audit()
{
  // Check if we can make audits
  // Are there currently MAX_AUDIT_NUM audits going on?
  bool all_set = true;
  for(int i = 0; i < MAX_AUDIT_NUM; i++) {
    if(g_info.curr_audits[i] == -1) {
      all_set = false;
      break;
    }
  }
  if(all_set) {
    popup_notify_goto_dialog(_("You can't start a new accusation"),
                             _("You can't start a new accusation because there "
                               "are already the maximum number of accusations "
                               "going on. Wait for one of these to "
                               "finish to begin a new one."),
                               nullptr, nullptr);
    return;
  }

  // Collect all actionable sabotage reports
  auto reports = sabotages_report::instance()->get_actionable_sabotages();
  if (reports.size() == 0) {
    popup_notify_goto_dialog(_("You can't start a new accusation"),
                             _("You don't have any sabotages you can act upon:\n\n"
                               "- You can only accuse someone if they steal from you,"
                               " and not if they only investigated you.\n"
                               "- You're already began an accusation on all your"
                               " actionable sabotages.\n"
                               "- Past accusations where the jury did not reach a"
                               " decision become invalid."),
                               nullptr, nullptr);
    return;
  }

  // Create dialog
  hud_multiple_selection_box *ask = new hud_multiple_selection_box(this);
  QVector<QString> options;
  char timestr[64];
  for(auto report : reports) {
    time_t now = report->timestamp;
    strftime(timestr, sizeof(timestr), "%d/%m %H:%M:%S", localtime(&now));
    options.push_back(QString::number(report->id) + "|" + QString(timestr) + ": " + QString(report->info));
  }
  ask->set_text_title_options(_("Select a sabotage to accuse"), _("Accusation"), options);
  ask->setAttribute(Qt::WA_DeleteOnClose);
  QObject::connect(ask, &hud_input_box::finished, [=](int result) {
    if (result == QDialog::Accepted) {
      QByteArray ask_bytes;

      QList<QListWidgetItem *> selected_items =
          ask->option_list.selectedItems();
      if (selected_items.size() == 0) {
        return;
      }

      ask_bytes = selected_items[0]->text().toLocal8Bit();
      QString data = ask_bytes.data();
      // Retrieve embedded ID
      int index = data.indexOf("|");
      if(index == -1) {
        return;
      }
      int id = data.left(index).toInt();
      log_warning("Selected sabotage %d", id);

      begin_audit_sabotage_selected(id);
    }
  });
  ask->show();
}

void government_report::begin_audit_sabotage_selected(int id)
{
  // Validation was done at this point, no worries
  // Now, ask which player to accuse
  auto reports = sabotages_report::instance()->get_actionable_sabotages();
  struct packet_sabotage_info_self *report = nullptr;
  for(auto r : reports) {
    if(r->id == id) {
      report = r;
      break;
    }
  }
  if(!report) {
    log_error("Couldn't find report %d", id);
    return;
  }

  // Create dialog
  hud_multiple_selection_box *ask = new hud_multiple_selection_box(this);
  QVector<QString> options;
  std::string raw_options[] = {
    "Yellow", "Blue", "Purple", "Green"
  };
  for(std::string option : raw_options) {
    if(player_id_from_string(client_player()->name) == player_id_from_string(option)) {
      continue;
    }
    options.push_back(QString(option.c_str()));
  }
  ask->set_text_title_options(_("Select a player to accuse"),
                              _("Accusation"), options);
  ask->setAttribute(Qt::WA_DeleteOnClose);
  QObject::connect(ask, &hud_input_box::finished, [=](int result) {
    if (result == QDialog::Accepted) {
      QByteArray ask_bytes;

      QList<QListWidgetItem *> selected_items =
          ask->option_list.selectedItems();
      if (selected_items.size() == 0) {
        return;
      }

      ask_bytes = selected_items[0]->text().toLocal8Bit();
      QString player = ask_bytes.data();
      log_warning("Selected player %s", player.toUtf8().data());

      confirm_audit_sabotage_selected(report, player);
    }
  });
  ask->show();
}

void government_report::confirm_audit_sabotage_selected(
  struct packet_sabotage_info_self *report, QString player)
{
  // Confirm with the player
  hud_message_box* a_audit_confirm = new hud_message_box(this);
  a_audit_confirm->setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes);
  a_audit_confirm->setDefaultButton(QMessageBox::Cancel);
  a_audit_confirm->set_text_title(
    QString("Are you sure you want to accuse %1?\nYou'll need to successfully convince the remaining players that you're right, or you may be penalized.\n").arg(player),
    _("Accusation")
  );
  QObject::connect(a_audit_confirm, &hud_message_box::finished, [=](int result) {
    if (result == QMessageBox::Yes) {
      log_warning("Accusation will move on!");

      struct packet_government_audit_start *req =
          new packet_government_audit_start();

      req->sabotage_id = report->id;
      req->accused_id = player_id_from_string(player.toUtf8().data());

      send_packet_government_audit_start(&client.conn, req);
    }
  });
  a_audit_confirm->show();
}

void government_report::show_audit_screen(int id)
{
  struct government_audit_info *info = g_info.find_cached_audit(id);
  if (!info) {
    log_warning("info is null");
    return;
  }

  layout->setCurrentIndex(1);
  curr_audit = info;

  // Convert audit id to chat idx
  int chat_idx = -1;
  for(int i = 0; i < MAX_AUDIT_NUM; i++) {
    if(g_info.curr_audits[i] == id) {
      chat_idx = i;
      break;
    }
  }
  if(chat_idx != -1) {
    chat_widgets[chat_idx]->set_filter(QString(CHAT_AUDIT_PREFIX) + QString::number(id) + ":");
    for(int i = 0; i < MAX_AUDIT_NUM; i++) {
      chat_widgets[i]->setVisible(i == chat_idx);
    }
  } else {
    log_error("Couldn't find chat window!");
  }

  log_warning("accuser %d accused %d me %d", info->accuser_id,
              info->accused_id, get_player_id(client.conn.playing));

  std::string accuser_name =
      player_id_to_string((player_id) info->accuser_id);
  std::string accused_name =
      player_id_to_string((player_id) info->accused_id);

  a_description->setText(QString(_("%1 vs %2"))
                              .arg(accuser_name.c_str())
                              .arg(accused_name.c_str()));

  // TODO: Info about sabotage type
  player_id my_id = get_player_id(client.conn.playing);
  if (my_id == info->accuser_id) {
    a_player_description->setText(
        QString(_("You are accusing %1 of %2. Convice the jury to rule in "
                  "your favor!"))
            .arg(accused_name.c_str())
            .arg("sabotage"));
  } else if (my_id == info->accused_id) {
    a_player_description->setText(
        QString(_("You are being accused of %1 by %2. You must convince "
                  "the jury that you're innocent!"))
            .arg("sabotage")
            .arg(accuser_name.c_str()));
  } else {
    a_player_description->setText(
        QString(_("You are a jury member in the trial of %1 vs %2. Seek "
                  "the truth from both players and determine who is "
                  "innocent and who will be penalized!"))
            .arg(accuser_name.c_str())
            .arg(accused_name.c_str()));
  }

  a_accuser_pixmap = get_player_thumb_sprite(tileset, info->accuser_id);
  a_accuser_pixmap_cont->setPixmap(*a_accuser_pixmap);

  a_accused_pixmap = get_player_thumb_sprite(tileset, info->accused_id);
  a_accused_pixmap_cont->setPixmap(*a_accused_pixmap);

  a_jury_1_pixmap = get_player_thumb_sprite(tileset, determine_jury_id(
                                                        (player_id)info->accuser_id,
                                                        (player_id)info->accused_id,
                                                        1));
  a_jury_1_pixmap_cont->setPixmap(*a_jury_1_pixmap);

  a_jury_2_pixmap = get_player_thumb_sprite(tileset, determine_jury_id(
                                                        (player_id)info->accuser_id,
                                                        (player_id)info->accused_id,
                                                        2));
  a_jury_2_pixmap_cont->setPixmap(*a_jury_2_pixmap);
}

void government_report::confirm_vote(audit_vote_type intended_vote)
{
  this->intended_vote = intended_vote;
  QString title, text;

  switch(intended_vote) {
    case AUDIT_VOTE_YES:
      title = "Vote \"true\"";
      text = "You're about to vote on the side of the accuser.\n\nYou agree that the accuser is right, and the accused must be penalized.\nIf your rulling is the truth, you'll receive a bonus.\nHowever, if this isn't the truth, you'll be convicting an innocent person, and will thus be penalized.\n\nAre you sure you want to vote \"true\"?";
      break;
    case AUDIT_VOTE_NO:
      title = "Vote \"false\"";
      text = "You're about to vote on the side of the accused.\n\nYou agree that the accused is innocent, and the accuser must be penalized.\nIf your rulling is the truth, you'll receive a bonus.\nHowever, if this isn't the truth, you'll be forgiving a guilty person, and will thus be penalized.\n\nAre you sure you want to vote \"false\"?";
      break;
    case AUDIT_VOTE_ABSTAIN:
      title = "Abstain";
      text = "You're about to abstain from voting.\n\nYou're not sure who is guilty, and don't want to risk voting on the wrong side.\nYou won't be penalized, but you'll also not gain anything from this audit.\n\nAre you sure you want to abstain?";
      break;
    case AUDIT_VOTE_NONE:
      break;
  }

  a_vote_confirm->set_text_title(text, title);
  a_vote_confirm->show();
}

void government_report::update_info()
{
  if(g_info.last_message_id != cached_last_message_id) {
    for(int i = cached_last_message_id + 1; i <= g_info.last_message_id; i++) {
      struct packet_government_news_req *req =
          new packet_government_news_req();
      req->id = i;

      send_packet_government_news_req(&client.conn, req);
    }
    cached_last_message_id = g_info.last_message_id;
  }

  if(g_info.last_audit_id != cached_last_audit_id) {
    for (int i = cached_last_audit_id + 1; i <= g_info.last_audit_id; i++) {
      struct packet_government_audit_info_req *req =
          new packet_government_audit_info_req();
      req->id = i;

      send_packet_government_audit_info_req(&client.conn, req);
    }
    cached_last_audit_id = g_info.last_audit_id;
  }

  for (int i = 0; i < MAX_AUDIT_NUM; i++) {
    m_auditing_buttons[i]->set_audit_id(g_info.curr_audits[i]);
  }
}

void government_report::update_news(struct government_news *news)
{
  cached_last_message_id = MAX(cached_last_message_id, news->id);
  QLabel* news_label = new QLabel();
  char timestr[64];
  time_t now = news->timestamp;
  strftime(timestr, sizeof(timestr), "%d/%m %H:%M:%S", localtime(&now));
  news_label->setText(QString(timestr) + ": " + QString(news->news));
  m_recent_decisions_scroll->layout()->addWidget(news_label);
}

void government_report::update_audit_info(struct government_audit_info *info)
{
  for(int i = 0; i < MAX_AUDIT_NUM; i++) {
    if(g_info.curr_audits[i] == info->id) {
      m_auditing_buttons[i]->set_audit_info(info);
    }
  }
}
