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

// common
#include "game.h"
// client
#include "client_main.h"
#include "climisc.h"
#include "helpdlg.h"
#include "text.h"
#include "tileset/sprite.h"
// gui-qt
#include "citydlg.h"
#include "fc_client.h"
#include "page_game.h"
#include "tooltips.h"
#include "top_bar.h"
#include "views/view_government.h"

government_report* government_report::_instance = nullptr;

government_report *government_report::instance()
{
  if (!_instance) {
    _instance = new government_report();
  }
  return _instance;
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
  m_auditing_start_button->setText(_("Initiate an audit"));
  m_layout->addWidget(m_auditing_start_button, 9, 4, 1, -1, Qt::AlignLeft);

  QWidget *m_auditing_widget = new QWidget();
  QHBoxLayout *m_auditing_layout = new QHBoxLayout();
  m_auditing_widget->setSizePolicy(size_expand_policy);
  m_auditing_widget->setLayout(m_auditing_layout);
  m_layout->addWidget(m_auditing_widget, 10, 4, 7, -1);

  m_auditing_buttons = new QPushButton*[m_auditing_count];
  for(int i = 0; i < m_auditing_count; i++) {
    m_auditing_buttons[i] = new QPushButton();
    m_auditing_buttons[i]->setSizePolicy(size_expand_policy);
    m_auditing_layout->addWidget(m_auditing_buttons[i]);
    if(i != m_auditing_count - 1) {
      m_auditing_layout->addSpacing(45);
    }
  }

  main_screen->setSizePolicy(size_expand_policy);
  main_screen->setLayout(m_layout);

  // Auditing screen
  QWidget *auditing_screen = new QWidget();
  QGridLayout *a_layout = new QGridLayout();
  QLabel *a_gov_label = new QLabel(_("Auditing"));
  a_gov_label->setSizePolicy(size_expand_policy);
  a_layout->addWidget(a_gov_label, 0, 0, -1, -1);
  auditing_screen->setSizePolicy(size_expand_policy);
  auditing_screen->setLayout(a_layout);

  // Add it all up
  layout->addWidget(main_screen);
  layout->addWidget(auditing_screen);
  setLayout(layout);

  // DEBUG
  //layout->setCurrentIndex(1);
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

/**
   Schedules paint event in some qt queue
 */
void government_report::redraw() { update(); }

void government_report::update_info()
{
  if(g_info.last_message_id == cached_last_message_id) {
    // TODO: Make requests for missing ids
  }

  if(g_info.last_audit_id == cached_last_audit_id) {
    // TODO: Make requests for missing ids
  }

    for (int i = 0; i < MAX_AUDIT_NUM; i++) {
      int id = g_info.curr_audits[i];
      if (id > -1) {
        m_auditing_buttons[i]->setText(QString::number(id));
        m_auditing_buttons[i]->setEnabled(true);
      } else {
        m_auditing_buttons[i]->setText(
            _("No auditing occuring\nin this slot"));
        m_auditing_buttons[i]->setEnabled(false);
      }
  }
}

void government_report::update_news(int id, int turn, const QString &news)
{
  cached_last_message_id = MAX(cached_last_message_id, id);
  QLabel* news_label = new QLabel();
  news_label->setText(QString::number(turn) + ": " + news);
  m_recent_decisions_scroll->layout()->addWidget(news_label);
}

void update_government_info()
{
  government_report::instance()->update_info();
}

void update_government_news(int id, int turn, const char *news)
{
  government_report::instance()->update_news(id, turn, QString(news));
}
