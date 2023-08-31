/**************************************************************************
 Copyright (c) 1996-2023 Freeciv21 and Freeciv contributors. This file is
 part of Freeciv21. Freeciv21 is free software: you can redistribute it
 and/or modify it under the terms of the GNU  General Public License  as
 published by the Free Software Foundation, either version 3 of the
 License,  or (at your option) any later version. You should have received
 a copy of the GNU General Public License along with Freeciv21. If not,
 see https://www.gnu.org/licenses/.
**************************************************************************/
#pragma once

// utility
#include "fc_types.h"
// common
#include "government.h"
// client
#include "chatline.h"
#include "hudwidget.h"
#include "repodlgs_g.h"

class QComboBox;
class QGridLayout;
class QStackedLayout;
class QLabel;
class QMouseEvent;
class QObject;
class QPaintEvent;
class QScrollArea;
class progress_bar;

class audit_button : public QPushButton {
  Q_OBJECT

public:
  audit_button();
  int get_audit_id() const;
  void set_audit_id(int id);
  void set_audit_info(struct government_audit_info *info);

signals:
  void audit_selected(int id);

private:
  int audit_id = -1;
};

/****************************************************************************
  Widget embedded as tab on game view (F7 default)
  Uses string "GOV" to mark it as opened
  You can check it using if (queen()->is_repo_dlg_open("GOV"))
****************************************************************************/
class government_report : public QWidget {
  Q_OBJECT

  QStackedLayout *layout;

  QScrollArea *m_recent_decisions_scroll;
  int m_auditing_count = MAX_AUDIT_NUM;
  audit_button **m_auditing_buttons;

  QLabel *a_description, *a_player_description;
  QLabel *a_accuser_pixmap_cont, *a_accused_pixmap_cont, *a_jury_1_pixmap_cont, *a_jury_2_pixmap_cont;
  QPixmap *a_accuser_pixmap, *a_accused_pixmap, *a_jury_1_pixmap, *a_jury_2_pixmap;
  QPushButton *a_jury_vote_yes, *a_jury_vote_no, *a_jury_vote_abstain;
  QLabel *a_consequence_good, *a_consequence_bad;
  hud_message_box *a_vote_confirm;

  chat_widget *chat_widgets[MAX_AUDIT_NUM];

  audit_vote_type intended_vote = AUDIT_VOTE_ABSTAIN;

  int cached_last_message_id = -1;
  int cached_last_audit_id = -1;

  struct government_audit_info *curr_audit = nullptr;

public:
  void init(bool raise);
  void redraw();
  void update_report();

protected:
  static government_report *_instance;

  void begin_audit();
  void begin_audit_sabotage_selected(int id);
  void confirm_audit_sabotage_selected(struct packet_sabotage_info_self *report,
                                       QString player);
  void show_audit_screen(int id);
  void confirm_vote(audit_vote_type intended_vote);

public:
  static government_report *instance();

  void update_info();
  void update_news(struct government_news *news);
  void update_audit_info(struct government_audit_info *info);

private:
  government_report();
  ~government_report();
  government_report(const government_report &other) = delete;
  government_report &operator=(const government_report &other) = delete;
  int index{0};
};
