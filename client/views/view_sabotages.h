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
#include "repodlgs_g.h"
#include "hudwidget.h"

class QComboBox;
class QGridLayout;
class QStackedLayout;
class QLabel;
class QMouseEvent;
class QObject;
class QPaintEvent;
class QScrollArea;
class progress_bar;

/****************************************************************************
  Widget embedded as tab on game view (F8 default)
  Uses string "SAB" to mark it as opened
  You can check it using if (queen()->is_repo_dlg_open("SAB"))
****************************************************************************/
class sabotages_report : public QWidget {
  Q_OBJECT

  QStackedLayout *layout;

  QScrollArea *m_sabotages_self_scroll;
  QScrollArea *m_sabotages_other_scroll;

  QWidget *m_sabotages_self_widget;
  QWidget *m_sabotages_other_widget;

  int cached_last_self_id = -1;
  int cached_last_other_id = -1;

  QVector<struct packet_sabotage_info_self*> m_sabotages_self;

public:
  void init(bool raise);
  void redraw();
  void reset();

protected:
  static sabotages_report *_instance;

public:
  static sabotages_report *instance();

  void update_info(int last_sabotage_self_id, int last_sabotage_other_id);
  void update_self_info(const struct packet_sabotage_info_self *info);
  void update_other_info(const struct packet_sabotage_info_other *info);

  struct packet_sabotage_info_self* find_cached_sabotage(int id) {
    auto iter = std::find_if(
        m_sabotages_self.begin(), m_sabotages_self.end(),
        [id](struct packet_sabotage_info_self *sabotage) { return sabotage->id == id; });
    if (iter != m_sabotages_self.end()) {
      return *iter;
    } else {
      return nullptr;
    }
  }
  QVector<struct packet_sabotage_info_self*> get_actionable_sabotages();

private:
  sabotages_report();
  ~sabotages_report();
  sabotages_report(const sabotages_report &other) = delete;
  sabotages_report &operator=(const sabotages_report &other) = delete;
  int index{0};
};
