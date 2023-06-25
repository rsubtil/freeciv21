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

// Qt
#include <QComboBox>
#include <QGridLayout>
#include <QStackedLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>

// common
#include "game.h"
// client
#include "chatline.h"
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
#include "views/view_sabotages.h"

sabotages_report* sabotages_report::_instance = nullptr;

sabotages_report *sabotages_report::instance()
{
  if (!_instance) {
    _instance = new sabotages_report();
  }
  return _instance;
}

/**
   Consctructor for sabotages_report
 */
sabotages_report::sabotages_report() : QWidget()
{
  QSize size;
  QSizePolicy size_expand_policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  QSizePolicy size_expand_y_policy(QSizePolicy::Minimum, QSizePolicy::Expanding);
  QSizePolicy size_expand_x_policy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  QSizePolicy size_fixed_policy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  // Main screen
  QGridLayout* layout = new QGridLayout();
  QLabel* m_sabotages_label = new QLabel(_("Sabotages"));
  m_sabotages_label->setSizePolicy(size_fixed_policy);
  m_sabotages_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  layout->addWidget(m_sabotages_label, 0, 0, -1, 4);

  QLabel* m_sabotages_self_label = new QLabel(_("Others to me:"));
  m_sabotages_self_label->setSizePolicy(size_fixed_policy);
  layout->addWidget(m_sabotages_self_label, 0, 4, 1, -1);

  m_sabotages_self_scroll = new QScrollArea();
  m_sabotages_self_scroll->setSizePolicy(size_expand_policy);
  m_sabotages_self_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_sabotages_self_scroll->setWidgetResizable(true);
  m_sabotages_self_widget = new QWidget();
  QVBoxLayout *m_sabotages_self_layout = new QVBoxLayout(m_sabotages_self_widget);
  m_sabotages_self_layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_sabotages_self_scroll->setWidget(m_sabotages_self_widget);
  layout->addWidget(m_sabotages_self_scroll, 1, 4, 7, -1);

  QLabel *m_sabotages_other_label = new QLabel(_("Me to others:"));
  m_sabotages_other_label->setSizePolicy(size_fixed_policy);
  layout->addWidget(m_sabotages_other_label, 8, 4, 1, -1);

  m_sabotages_other_scroll = new QScrollArea();
  m_sabotages_other_scroll->setSizePolicy(size_expand_policy);
  m_sabotages_other_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_sabotages_other_scroll->setWidgetResizable(true);
  m_sabotages_other_widget = new QWidget();
  QVBoxLayout *m_sabotages_other_layout = new QVBoxLayout(m_sabotages_other_widget);
  m_sabotages_other_scroll->setWidget(m_sabotages_other_widget);
  m_sabotages_other_layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  layout->addWidget(m_sabotages_other_scroll, 9, 4, 7, -1);

  // Add it all up
  setLayout(layout);
}

/**
   Destructor for sabotages report
   Removes "SAB" string marking it as closed
   And frees given index on list marking it as ready for new widget
 */
sabotages_report::~sabotages_report()
{
  queen()->removeRepoDlg(QStringLiteral("SAB"));
}

/**
   Updates sabotages_report and marks it as opened
   It has to be called soon after constructor.
   It could be in constructor but compiler will yell about not used variable
 */
void sabotages_report::init(bool raise)
{
  Q_UNUSED(raise)
  queen()->gimmePlace(this, QStringLiteral("SAB"));
  index = queen()->addGameTab(this);
  queen()->game_tab_widget->setCurrentIndex(index);
}

/**
   Schedules paint event in some qt queue
 */
void sabotages_report::redraw() { update(); }

void sabotages_report::reset()
{
  if(m_sabotages_self_widget->layout()) {
    QLayoutItem *child;
    while ((child = m_sabotages_self_widget->layout()->takeAt(0)) != nullptr) {
      delete child->widget();
      delete child;
    }
  }
  if(m_sabotages_other_widget->layout()) {
    QLayoutItem *child;
    while ((child = m_sabotages_other_widget->layout()->takeAt(0)) != nullptr) {
      delete child->widget();
      delete child;
    }
  }
  cached_last_self_id = -1;
  cached_last_other_id = -1;
}

void sabotages_report::update_info(int last_sabotage_self_id, int last_sabotage_other_id)
{
  if (last_sabotage_self_id != cached_last_self_id) {
    for (int i = cached_last_self_id + 1;
         i <= last_sabotage_self_id; i++) {
      struct packet_sabotage_info_self_req *req =
          new packet_sabotage_info_self_req();
      req->id = i;

      send_packet_sabotage_info_self_req(&client.conn, req);
    }
    cached_last_self_id = last_sabotage_self_id;
  }

  if (last_sabotage_other_id != cached_last_other_id) {
    for (int i = cached_last_other_id + 1;
         i <= last_sabotage_other_id; i++) {
      struct packet_sabotage_info_other_req *req =
          new packet_sabotage_info_other_req();
      req->id = i;

      send_packet_sabotage_info_other_req(&client.conn, req);
    }
    cached_last_other_id = last_sabotage_other_id;
  }
}

void sabotages_report::update_self_info(int id, int turn, const char *info)
{
  cached_last_self_id = MAX(cached_last_self_id, id);
  QLabel *label = new QLabel();
  label->setWordWrap(true);
  label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  label->setText(QString::number(turn) + ": " + QString(info));
  m_sabotages_self_widget->layout()->addWidget(label);
}

void sabotages_report::update_other_info(int id, int turn, const char *info)
{
  cached_last_other_id = MAX(cached_last_other_id, id);
  QLabel *label = new QLabel();
  label->setWordWrap(true);
  label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  label->setText(QString::number(turn) + ": " + QString(info));
  m_sabotages_other_widget->layout()->addWidget(label);
}
