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
// client
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

/****************************************************************************
  Widget embedded as tab on game view (F7 default)
  Uses string "GOV" to mark it as opened
  You can check it using if (queen()->is_repo_dlg_open("GOV"))
****************************************************************************/
class government_report : public QWidget {
  Q_OBJECT

  QStackedLayout *layout;
  QScrollArea *m_recent_decisions_scroll;
  int m_auditing_count = 3;
  QPushButton **m_auditing_buttons;

      public : government_report();
  ~government_report() override;
  void update_report();
  void init(bool raise);
  void redraw();

private:
  int index{0};
};
