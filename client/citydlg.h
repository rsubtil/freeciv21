/**************************************************************************
 Copyright (c) 1996-2020 Freeciv21 and Freeciv contributors. This file is
 part of Freeciv21. Freeciv21 is free software: you can redistribute it
 and/or modify it under the terms of the GNU  General Public License  as
 published by the Free Software Foundation, either version 3 of the
 License,  or (at your option) any later version. You should have received
 a copy of the GNU General Public License along with Freeciv21. If not,
 see https://www.gnu.org/licenses/.
**************************************************************************/
#pragma once

#include "fc_types.h"

// Qt
#include <QDialog>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QItemDelegate>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QTableWidget>
#include <QToolTip>
#include <QtMath>
#include <qobjectdefs.h>
// gui-qt
#include "dialogs.h"

class QAction;
class QCheckBox;
class QCloseEvent;
class QContextMenuEvent;
class QEvent;
class QFont;
class QGridLayout;
class QGroupBox;
class QHBoxLayout;
class QHideEvent;
class QItemSelection;
class QMenu;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QPushButton;
class QRadioButton;
class QRect;
class QResizeEvent;
class QShowEvent;
class QSplitter;
class QTableWidget;
class QTableWidgetItem;
class QTimerEvent;
class QVBoxLayout;
class QVariant;
class fc_tooltip;
class QPixmap;

/****************************************************************************
  A list widget that sets its size hint to the size of its contents.
****************************************************************************/
class unit_list_widget : public QListWidget {
  Q_OBJECT

public:
  explicit unit_list_widget(QWidget *parent = nullptr);
  QSize viewportSizeHint() const override;

  /// Sets whether the list should try to use a single line.
  void set_oneliner(bool oneliner) { m_oneliner = oneliner; }

  /// Sets whether upkeep needs to be shown.
  void set_show_upkeep(bool show) { m_show_upkeep = show; }

  void set_units(unit_list *units);
  std::vector<unit *> selected_playable_units() const;

protected:
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  void activate();
  QPixmap create_unit_image(const unit *punit);

private:
  bool m_oneliner = false;
  bool m_show_upkeep = false;
};

/****************************************************************************
  Custom progressbar with animated progress and right click event
****************************************************************************/
class progress_bar : public QProgressBar {
  Q_OBJECT
signals:
  void clicked();

public:
  progress_bar(QWidget *parent);
  ~progress_bar() override;
  void mousePressEvent(QMouseEvent *event) override
  {
    Q_UNUSED(event);
    emit clicked();
  }
  void set_pixmap(struct universal *target);
  void set_pixmap(int n);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void create_region();
  QPixmap *pix;
  QRegion reg;
  QFont *sfont;
};

/****************************************************************************
  Item delegate for production popup
****************************************************************************/
class city_production_delegate : public QItemDelegate {
  Q_OBJECT

public:
  city_production_delegate(QPoint sh, QObject *parent, struct city *city);
  ~city_production_delegate() override = default;
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;

private:
  int item_height;
  QPoint pd;
  struct city *pcity;

protected:
  void drawFocus(QPainter *painter, const QStyleOptionViewItem &option,
                 const QRect &rect) const override;
};

/****************************************************************************
  Single item in production popup
****************************************************************************/
class production_item : public QObject {
  Q_OBJECT

public:
  production_item(struct universal *ptarget, QObject *parent);
  ~production_item() override;
  inline int columnCount() const { return 1; }
  QVariant data() const;

private:
  struct universal *target;
};

/***************************************************************************
  City production model
***************************************************************************/
class city_production_model : public QAbstractListModel {
  Q_OBJECT

public:
  city_production_model(struct city *pcity, bool f, bool su, bool sw,
                        bool sb, QObject *parent = 0);
  ~city_production_model() override;
  inline int
  rowCount(const QModelIndex &index = QModelIndex()) const override
  {
    Q_UNUSED(index);
    return (qCeil(static_cast<float>(city_target_list.size()) / 3));
  }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override
  {
    Q_UNUSED(parent);
    return 3;
  }
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  void populate();
  QPoint sh;

private:
  QList<production_item *> city_target_list;
  struct city *mcity;
  bool future_t;
  bool show_units;
  bool show_buildings;
  bool show_wonders;
};

/****************************************************************************
  Class for popup avaialable production
****************************************************************************/
class production_widget : public QTableView {
  Q_OBJECT

  city_production_model *list_model;
  city_production_delegate *c_p_d;

public:
  production_widget(QWidget *parent, struct city *pcity, bool future,
                    int when, int curr, bool show_units, bool buy = false,
                    bool show_wonders = true, bool show_buildings = true);
  ~production_widget() override;

public slots:
  void prod_selected(const QItemSelection &sl, const QItemSelection &ds);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *ev) override;

private:
  struct city *pw_city;
  int when_change;
  int curr_selection;
  bool buy_it;
  fc_tooltip *fc_tt;
};

class cityIconInfoLabel : public QWidget {
  Q_OBJECT

public:
  cityIconInfoLabel(QWidget *parent = 0);
  void setCity(struct city *pcity);
  void updateText();

private:
  void initLayout();
  struct city *pcity{nullptr};
  QLabel labs[6];
  int pixHeight;
};

class city_info : public QWidget {
  Q_OBJECT

public:
  city_info(QWidget *parent = 0);
  void update_labels(struct city *ci_city);

private:
  QLabel *m_size, *m_food, *m_production, *m_trade, *m_gold, *m_science_acc,
      *m_materials, *m_luxury, *m_science, *m_granary, *m_growth,
      *m_corruption, *m_waste, *m_culture, *m_pollution, *m_plague_label,
      *m_plague, *m_stolen, *m_airlift;
};

#include "ui_citydlg.h"
/****************************************************************************
  City dialog
****************************************************************************/
class city_dialog : public QWidget {

  Q_OBJECT
  Q_DISABLE_COPY(city_dialog);
  Ui::FormCityDlg ui;
  bool future_targets{false}, show_units{true}, show_wonders{true},
      show_buildings{true};
  int selected_row_p;

public:
  city_dialog(QWidget *parent = 0);

  ~city_dialog() override;
  void setup_ui(struct city *qcity);
  void refresh();
  struct city *pcity = nullptr;
  bool dont_focus{false};

private:
  void update_title();
  void update_building();
  void update_info_label();
  void update_improvements();
  void update_units();
  void update_nation_table();
  void update_disabled();
  void update_sliders();
  void change_production(bool next);

private slots:
  void next_city();
  void prev_city();
  void get_city(bool next);
  void show_targets();
  void show_targets_worklist();
  void dbl_click_p(QTableWidgetItem *item);
  void disband_state_changed(bool allow_disband);
  void city_rename();

protected:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;
};

void destroy_city_dialog();
void city_font_update();
