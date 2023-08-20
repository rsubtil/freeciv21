/*
 Copyright (c) 1996-2023 Freeciv21 and Freeciv contributors. This file is
 part of Freeciv21. Freeciv21 is free software: you can redistribute it
 and/or modify it under the terms of the GNU  General Public License  as
 published by the Free Software Foundation, either version 3 of the
 License,  or (at your option) any later version. You should have received
 a copy of the GNU General Public License along with Freeciv21. If not,
 see https://www.gnu.org/licenses/.
 */

#include "citydlg.h"
// Qt
#include <QApplication>
#include <QCheckBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpacerItem>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidgetAction>
// utility
#include "fc_types.h"
#include "fcintl.h"
#include "support.h"
// common
#include "city.h"
#include "game.h"

// client
#include "citydlg_common.h"
#include "client_main.h"
#include "climisc.h"
#include "control.h"
#include "global_worklist.h"
#include "governor.h"
#include "mapctrl_common.h"
#include "tileset/tilespec.h"
#include "views/view_map_common.h"
// gui-qt
#include "canvas.h"
#include "fc_client.h"
#include "fonts.h"
#include "hudwidget.h"
#include "icons.h"
#include "page_game.h"
#include "qtg_cxxside.h"
#include "text.h"
#include "tooltips.h"
#include "top_bar.h"
#include "unitlist.h"
#include "utils/improvement_seller.h"
#include "utils/unit_quick_menu.h"
#include "views/view_cities.h" // hIcon
#include "views/view_map.h"
#include "widgets/city/governor_widget.h"

extern QString split_text(const QString &text, bool cut);
extern QString cut_helptext(const QString &text);

/**
   Constructor
 */
unit_list_widget::unit_list_widget(QWidget *parent) : QListWidget(parent)
{
  // Make sure viewportSizeHint is used
  setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
  setWrapping(true);
  setMovement(QListView::Static);

  connect(this, &QListWidget::itemDoubleClicked, this,
          &unit_list_widget::activate);
}

/**
   Reimplemented virtual method.
 */
QSize unit_list_widget::viewportSizeHint() const
{
  if (!m_oneliner) {
    return QSize(1, 5555);
  }
  // Try to put everything on one line
  QSize hint;
  for (int i = 0; i < count(); ++i) {
    hint = hint.expandedTo(sizeHintForIndex(indexFromItem(item(i))));
  }
  hint.setWidth(hint.width() * count());
  return hint;
}

/**
 * Sets the list of units to be displayed.
 */
void unit_list_widget::set_units(unit_list *units)
{
  setUpdatesEnabled(false);
  clear();

  QSize icon_size;
  unit_list_iterate(units, punit)
  {
    auto *item = new QListWidgetItem();
    item->setToolTip(unit_description(punit));
    item->setData(Qt::UserRole, punit->id);

    auto pixmap = create_unit_image(punit);
    icon_size = icon_size.expandedTo(pixmap.size());
    item->setIcon(QIcon(pixmap));
    addItem(item);
  }
  unit_list_iterate_end;

  setGridSize(icon_size);
  setIconSize(icon_size);

  setUpdatesEnabled(true);
  updateGeometry();
}

/**
 * Finds the list of currently selected units
 */
std::vector<unit *> unit_list_widget::selected_playable_units() const
{
  if (!can_client_issue_orders()) {
    return {};
  }

  auto units = std::vector<unit *>();
  for (const auto item : selectedItems()) {
    auto id = item->data(Qt::UserRole).toInt();
    const auto unit = game_unit_by_number(id);
    if (unit && unit_owner(unit) == client_player()) {
      units.push_back(unit);
    }
  }
  return units;
}

/**
 * Reimplemented to provide the unit context menu.
 */
void unit_list_widget::contextMenuEvent(QContextMenuEvent *event)
{
  const auto units = selected_playable_units();
  if (units.empty()) {
    return;
  }

  auto menu = new QMenu;
  menu->setAttribute(Qt::WA_DeleteOnClose);
  freeciv::add_quick_unit_actions(menu, units);
  menu->popup(event->globalPos());

  event->accept();
}

/**
 * Activates unit and closes city dialog
 */
void unit_list_widget::activate()
{
  const auto selection = selected_playable_units();

  unit_focus_set(nullptr); // Clear
  for (const auto unit : selection) {
    unit_focus_add(unit);
  }

  if (!selection.empty()) {
    queen()->city_overlay->dont_focus = true;
    popdown_city_dialog();
  }
}

/**
 * Creates the image to represent the given unit in the list
 */
QPixmap unit_list_widget::create_unit_image(const unit *punit)
{
  int happy_cost = 0;
  if (m_show_upkeep) {
    if (auto home = game_city_by_number(punit->homecity)) {
      auto free_unhappy = get_city_bonus(home, EFT_MAKE_CONTENT_MIL);
      happy_cost = city_unit_unhappiness(punit, &free_unhappy);
    }
  }

  double isosize = 0.6;
  if (tileset_hex_height(tileset) > 0 || tileset_hex_width(tileset) > 0) {
    isosize = 0.45;
  }

  auto unit_pixmap = QPixmap();
  if (punit) {
    if (m_show_upkeep) {
      unit_pixmap = QPixmap(tileset_unit_width(get_tileset()),
                            tileset_unit_with_upkeep_height(get_tileset()));
    } else {
      unit_pixmap = QPixmap(tileset_unit_width(get_tileset()),
                            tileset_unit_height(get_tileset()));
    }

    unit_pixmap.fill(Qt::transparent);
    put_unit(punit, &unit_pixmap, 0, 0);

    if (m_show_upkeep) {
      put_unit_city_overlays(punit, &unit_pixmap, 0,
                             tileset_unit_layout_offset_y(get_tileset()),
                             punit->upkeep, happy_cost);
    }
  }

  auto img = unit_pixmap.toImage();
  auto crop_rect = zealous_crop_rect(img);
  img = img.copy(crop_rect);

  if (tileset_is_isometric(tileset)) {
    return QPixmap::fromImage(
        img.scaledToHeight(tileset_unit_width(get_tileset()) * isosize,
                           Qt::SmoothTransformation));
  } else {
    return QPixmap::fromImage(img.scaledToHeight(
        tileset_unit_width(get_tileset()), Qt::SmoothTransformation));
  }
}

/**
   Custom progressbar constructor
 */
progress_bar::progress_bar(QWidget *parent) : QProgressBar(parent)
{
  create_region();
  sfont = new QFont;
  pix = nullptr;
}

/**
   Custom progressbar destructor
 */
progress_bar::~progress_bar()
{
  delete pix;
  delete sfont;
}

/**
   Custom progressbar resize event
 */
void progress_bar::resizeEvent(QResizeEvent *event)
{
  Q_UNUSED(event);
  create_region();
}

/**
   Sets pixmap from given universal for custom progressbar
 */
void progress_bar::set_pixmap(struct universal *target)
{
  const QPixmap *sprite;
  QImage cropped_img;
  QImage img;
  QPixmap tpix;
  QRect crop;

  if (VUT_UTYPE == target->kind) {
    sprite = get_unittype_sprite(get_tileset(), target->value.utype,
                                 direction8_invalid());
  } else {
    sprite = get_building_sprite(tileset, target->value.building);
  }
  delete pix;
  if (sprite == nullptr) {
    pix = nullptr;
    return;
  }
  img = sprite->toImage();
  crop = zealous_crop_rect(img);
  cropped_img = img.copy(crop);
  tpix = QPixmap::fromImage(cropped_img);
  pix = new QPixmap(tpix.width(), tpix.height());
  pix->fill(Qt::transparent);
  pixmap_copy(pix, &tpix, 0, 0, 0, 0, tpix.width(), tpix.height());
}

/**
   Sets pixmap from given tech number for custom progressbar
 */
void progress_bar::set_pixmap(int n)
{
  const QPixmap *sprite = nullptr;
  if (valid_advance_by_number(n)) {
    sprite = get_tech_sprite(tileset, n);
  }
  delete pix;
  if (sprite == nullptr) {
    pix = nullptr;
    return;
  }
  pix = new QPixmap(sprite->width(), sprite->height());
  pix->fill(Qt::transparent);
  pixmap_copy(pix, sprite, 0, 0, 0, 0, sprite->width(), sprite->height());
  if (isVisible()) {
    update();
  }
}

/**
   Paint event for custom progress bar
 */
void progress_bar::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event)
  QPainter p;
  QLinearGradient g, gx;
  QColor c;
  QRect r, rx, r2;
  int max;
  int f_size;
  int pix_width = 0;
  int point_size = sfont->pointSize();
  int pixel_size = sfont->pixelSize();

  if (pix != nullptr) {
    pix_width = height() - 4;
  }
  if (point_size < 0) {
    f_size = pixel_size;
  } else {
    f_size = point_size;
  }

  rx.setX(0);
  rx.setY(0);
  rx.setWidth(width());
  rx.setHeight(height());
  p.begin(this);
  p.drawLine(rx.topLeft(), rx.topRight());
  p.drawLine(rx.bottomLeft(), rx.bottomRight());

  max = maximum();

  if (max == 0) {
    max = 1;
  }

  r = QRect(0, 0, width() * value() / max, height());

  gx = QLinearGradient(0, 0, 0, height());
  c = QColor(palette().color(QPalette::Highlight));
  gx.setColorAt(0, c);
  gx.setColorAt(0.5, QColor(40, 40, 40));
  gx.setColorAt(1, c);
  p.fillRect(r, QBrush(gx));
  p.setClipRegion(reg);

  g = QLinearGradient(0, 0, width(), height());
  c.setAlphaF(0.1);
  g.setColorAt(0, c);
  c.setAlphaF(0.9);
  g.setColorAt(1, c);
  p.fillRect(r, QBrush(g));

  p.setClipping(false);
  r2 = QRect(width() * value() / max, 0, width(), height());
  c = palette().color(QPalette::Window);
  p.fillRect(r2, c);

  // draw icon
  if (pix != nullptr) {
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.drawPixmap(
        2, 2, pix_width * static_cast<float>(pix->width()) / pix->height(),
        pix_width, *pix, 0, 0, pix->width(), pix->height());
  }

  // draw text
  c = palette().color(QPalette::Text);
  p.setPen(c);
  sfont->setCapitalization(QFont::AllUppercase);
  sfont->setBold(true);
  p.setFont(*sfont);

  if (text().contains('\n')) {
    QString s1, s2;
    int i, j;

    i = text().indexOf('\n');
    s1 = text().left(i);
    s2 = text().right(text().count() - i);

    if (2 * f_size >= 2 * height() / 3) {
      if (point_size < 0) {
        sfont->setPixelSize(height() / 4);
      } else {
        sfont->setPointSize(height() / 4);
      }
    }

    j = height() - 2 * f_size;

    QFontMetrics fm(*sfont);
    if (fm.horizontalAdvance(s1) > rx.width()) {
      s1 = fm.elidedText(s1, Qt::ElideRight, rx.width());
    }

    i = rx.width() - fm.horizontalAdvance(s1) + pix_width;
    i = qMax(0, i);
    p.drawText(i / 2, j / 3 + f_size, s1);

    if (fm.horizontalAdvance(s2) > rx.width()) {
      s2 = fm.elidedText(s2, Qt::ElideRight, rx.width());
    }

    i = rx.width() - fm.horizontalAdvance(s2) + pix_width;
    i = qMax(0, i);

    p.drawText(i / 2, height() - j / 3, s2);
  } else {
    QString s;
    int i, j;
    s = text();
    j = height() - f_size;

    QFontMetrics fm(*sfont);
    if (fm.horizontalAdvance(s) > rx.width()) {
      s = fm.elidedText(s, Qt::ElideRight, rx.width());
    }

    i = rx.width() - fm.horizontalAdvance(s) + pix_width;
    i = qMax(0, i);
    p.drawText(i / 2, j / 2 + f_size, s);
  }
  p.end();
}

/**
   Creates region with diagonal lines
 */
void progress_bar::create_region()
{
  int offset;
  QRect r(-50, 0, width() + 50, height());
  int chunk_width = 16;
  int size = width() + 50;
  reg = QRegion();

  for (offset = 0; offset < (size * 2); offset += (chunk_width * 2)) {
    QPolygon a;

    a.setPoints(4, r.x(), r.y() + offset, r.x() + r.width(),
                (r.y() + offset) - size, r.x() + r.width(),
                (r.y() + offset + chunk_width) - size, r.x(),
                r.y() + offset + chunk_width);
    reg += QRegion(a);
  }
}

/**
   Draws X on pixmap pointing its useless
 */
static void pixmap_put_x(QPixmap *pix)
{
  QPen pen(QColor(0, 0, 0));
  QPainter p;

  pen.setWidth(2);
  p.begin(pix);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(pen);
  p.drawLine(0, 0, pix->width(), pix->height());
  p.drawLine(pix->width(), 0, 0, pix->height());
  p.end();
}

cityIconInfoLabel::cityIconInfoLabel(QWidget *parent) : QWidget(parent)
{
  auto f = fcFont::instance()->getFont(fonts::default_font);
  QFontMetrics fm(f);

  pixHeight = fm.height();

  initLayout();
}

void cityIconInfoLabel::setCity(city *pciti) { pcity = pciti; }

// inits icons only
void cityIconInfoLabel::initLayout()
{
  QHBoxLayout *l = new QHBoxLayout();
  labs[0].setPixmap(
      (hIcon::i()->get(QStringLiteral("gold"))).pixmap(pixHeight));
  l->addWidget(&labs[0]);
  l->addWidget(&labs[1]);

  labs[2].setPixmap(
      (hIcon::i()->get(QStringLiteral("science"))).pixmap(pixHeight));
  l->addWidget(&labs[2]);
  l->addWidget(&labs[3]);

  labs[4].setPixmap(
      (hIcon::i()->get(QStringLiteral("materials"))).pixmap(pixHeight));
  l->addWidget(&labs[4]);
  l->addWidget(&labs[5]);

  setLayout(l);
}

void cityIconInfoLabel::updateText()
{
  QString grow_time;
  if (!pcity) {
    return;
  }

  labs[1].setText(QString::number(pcity->surplus[O_GOLD]));
  labs[0].setToolTip(get_city_dialog_output_text(pcity, O_GOLD));
  labs[1].setToolTip(get_city_dialog_output_text(pcity, O_GOLD));

  labs[3].setText(QString::number(pcity->surplus[O_SCIENCE]));
  labs[2].setToolTip(get_city_dialog_output_text(pcity, O_SCIENCE));
  labs[3].setToolTip(get_city_dialog_output_text(pcity, O_SCIENCE));

  labs[5].setText(QString::number(pcity->surplus[O_MATERIALS]));
  labs[4].setToolTip(get_city_dialog_output_text(pcity, O_MATERIALS));
  labs[5].setToolTip(get_city_dialog_output_text(pcity, O_MATERIALS));
}

city_info::city_info(QWidget *parent) : QWidget(parent)
{
  QGridLayout *info_grid_layout = new QGridLayout();
  info_grid_layout->setHorizontalSpacing(6);
  info_grid_layout->setVerticalSpacing(0);
  info_grid_layout->setContentsMargins(0, 0, 0, 0);
  info_grid_layout->setColumnStretch(1, 100);
  setLayout(info_grid_layout);

  auto small_font = fcFont::instance()->getFont(fonts::notify_label);

  // We use this to shorten the code below, as it would otherwise be very
  // repetitive. It creates a label for the description and another for the
  // value, and returns the second.
  const auto create_labels = [&](const char *title) {
    auto label = new QLabel(title, this);
    label->setFont(small_font);
    label->setProperty(fonts::notify_label, "true");
    info_grid_layout->addWidget(label, info_grid_layout->rowCount(), 0);

    auto value = new QLabel(this);
    value->setFont(small_font);
    value->setProperty(fonts::notify_label, "true");
    info_grid_layout->addWidget(value, info_grid_layout->rowCount() - 1, 1);
    return std::make_pair(label, value);
  };

  QLabel *dummy;
  std::tie(dummy, m_gold) = create_labels(_("Gold:"));
  std::tie(dummy, m_science) = create_labels(_("Science:"));
  std::tie(dummy, m_materials) = create_labels(_("Materials:"));
}

void city_info::update_labels(struct city *pcity)
{
  m_size->setText(
      QString::asprintf("%3d (%s)", pcity->size,
                        qUtf8Printable(get_city_dialog_status_text(pcity))));
  m_size->setToolTip(get_city_dialog_size_text(pcity));

  m_food->setText(QString::asprintf(
      "<B style=\"white-space:pre\">%3d (%+4d)</B>",
      pcity->prod[O_FOOD] + pcity->waste[O_FOOD], pcity->surplus[O_FOOD]));
  m_food->setToolTip(get_city_dialog_output_text(pcity, O_FOOD));

  m_production->setText(
      QString::asprintf("<B style=\"white-space:pre\">%3d (%+4d)</B>",
                        pcity->prod[O_SHIELD] + pcity->waste[O_SHIELD],
                        pcity->surplus[O_SHIELD]));
  m_production->setToolTip(get_city_dialog_output_text(pcity, O_SHIELD));

  m_trade->setText(
      QString::asprintf("<B style=\"white-space:pre\">%3d (%+4d)</B>",
                        pcity->prod[O_TRADE] + pcity->waste[O_TRADE],
                        pcity->surplus[O_TRADE]));
  m_trade->setToolTip(get_city_dialog_output_text(pcity, O_TRADE));

  m_gold->setText(QString::asprintf(
      "%3d (%+4d)", pcity->prod[O_GOLD] + pcity->waste[O_GOLD],
      pcity->surplus[O_GOLD]));
  m_gold->setToolTip(get_city_dialog_output_text(pcity, O_GOLD));

  m_science_acc->setText(QString::asprintf(
      "%3d (%+4d)", pcity->prod[O_SCIENCE_ACC] + pcity->waste[O_SCIENCE_ACC],
      pcity->surplus[O_SCIENCE_ACC]));
  m_science_acc->setToolTip(get_city_dialog_output_text(pcity, O_SCIENCE_ACC));

  m_materials->setText(QString::asprintf(
      "%3d (%+4d)", pcity->prod[O_MATERIALS] + pcity->waste[O_MATERIALS],
      pcity->surplus[O_MATERIALS]));
  m_materials->setToolTip(get_city_dialog_output_text(pcity, O_MATERIALS));

  m_luxury->setText(QString::asprintf(
      "%3d (%+4d)", pcity->prod[O_LUXURY] + pcity->waste[O_LUXURY],
      pcity->surplus[O_LUXURY]));
  m_luxury->setToolTip(get_city_dialog_output_text(pcity, O_LUXURY));

  m_science->setText(QString::asprintf(
      "%3d (%+4d)", pcity->prod[O_SCIENCE] + pcity->waste[O_SCIENCE],
      pcity->surplus[O_SCIENCE]));
  m_science->setToolTip(get_city_dialog_output_text(pcity, O_SCIENCE));

  m_granary->setText(
      QString::asprintf("%3d/%-4d", pcity->food_stock,
                        city_granary_size(city_size_get(pcity))));

  m_growth->setText(get_city_dialog_growth_value(pcity));

  m_corruption->setText(QString::asprintf("%3d", pcity->waste[O_TRADE]));

  m_waste->setText(QString::asprintf("%3d", pcity->waste[O_SHIELD]));

  m_culture->setText(QString::asprintf("%3d", pcity->client.culture));
  m_culture->setToolTip(get_city_dialog_culture_text(pcity));

  m_pollution->setText(QString::asprintf("%3d", pcity->pollution));
  m_pollution->setToolTip(get_city_dialog_pollution_text(pcity));

  if (game.info.illness_on) {
    auto illness =
        city_illness_calc(pcity, nullptr, nullptr, nullptr, nullptr);
    // illness is in tenth of percent
    m_plague->setText(
        QString::asprintf("%3.1f%%", static_cast<float>(illness) / 10.0));
    m_plague->setToolTip(get_city_dialog_illness_text(pcity));
  }
  m_plague_label->setVisible(game.info.illness_on);
  m_plague->setVisible(game.info.illness_on);

  if (pcity->steal > 0) {
    m_stolen->setText(QString::asprintf(
        PL_("%3d time", "%3d times", pcity->steal), pcity->steal));
  } else {
    m_stolen->setText(_("Not stolen"));
  }

  m_airlift->setText(get_city_dialog_airlift_value(pcity));
  m_airlift->setToolTip(get_city_dialog_airlift_text(pcity));
}

/**
   Constructor for city_dialog, sets layouts, policies ...
 */
city_dialog::city_dialog(QWidget *parent) : QWidget(parent)

{
  QFont f = QApplication::font();
  QFontMetrics fm(f);

  int h = 2 * fm.height() + 2;
  auto small_font = fcFont::instance()->getFont(fonts::notify_label);
  ui.setupUi(this);

  // Prevent mouse events from going through the panels to the main map
  for (auto child : findChildren<QWidget *>()) {
    child->setAttribute(Qt::WA_NoMousePropagation);
  }

  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  setMouseTracking(true);
  selected_row_p = -1;
  pcity = nullptr;

  // main tab
  ui.bclose->setIcon(
      fcIcons::instance()->getIcon(QStringLiteral("city-close")));
  ui.bclose->setToolTip(_("Close city dialog"));
  connect(ui.bclose, &QAbstractButton::clicked, this, &QWidget::hide);
  ui.next_city_but->setIcon(
      fcIcons::instance()->getIcon(QStringLiteral("city-right")));
  ui.next_city_but->setToolTip(_("Show next city"));
  connect(ui.next_city_but, &QAbstractButton::clicked, this,
          &city_dialog::next_city);
  connect(ui.prev_city_but, &QAbstractButton::clicked, this,
          &city_dialog::prev_city);
  ui.prev_city_but->setIcon(
      fcIcons::instance()->getIcon(QStringLiteral("city-left")));
  ui.prev_city_but->setToolTip(_("Show previous city"));
  ui.production_combo_p->setToolTip(_("Click to change current production"));
  ui.production_combo_p->setFixedHeight(h);
  connect(ui.production_combo_p, &progress_bar::clicked, this,
          &city_dialog::show_targets);

  // governor tab
  ui.present_units_list->set_oneliner(true);

  installEventFilter(this);
}

/**
   Changes production to next one or previous
 */
void city_dialog::change_production(bool next)
{
  cid cprod;
  int i, pos;
  int item, targets_used;
  QList<cid> prod_list;
  struct item items[MAX_NUM_PRODUCTION_TARGETS];
  struct universal targets[MAX_NUM_PRODUCTION_TARGETS];
  struct universal univ;

  pos = 0;
  cprod = cid_encode(pcity->production);
  targets_used = collect_eventually_buildable_targets(targets, pcity, false);
  name_and_sort_items(targets, targets_used, items, false, pcity);

  for (item = 0; item < targets_used; item++) {
    if (can_city_build_now(pcity, &items[item].item)) {
      prod_list << cid_encode(items[item].item);
    }
  }

  for (i = 0; i < prod_list.size(); i++) {
    if (prod_list.at(i) == cprod) {
      if (next) {
        pos = i + 1;
      } else {
        pos = i - 1;
      }
    }
  }
  if (pos == prod_list.size()) {
    pos = 0;
  }
  if (pos == -1) {
    pos = prod_list.size() - 1;
  }
  univ = cid_decode(static_cast<cid>(prod_list.at(pos)));
  city_change_production(pcity, &univ);
}

/**
   Updates buttons/widgets which should be enabled/disabled
 */
void city_dialog::update_disabled()
{
  const auto can_edit =
      can_client_issue_orders() && city_owner(pcity) == client.conn.playing;
  ui.prev_city_but->setEnabled(can_edit);
  ui.next_city_but->setEnabled(can_edit);
  ui.production_combo_p->setEnabled(can_edit);
  ui.present_units_list->setEnabled(can_edit);
}

/**
   City dialog destructor
 */
city_dialog::~city_dialog()
{
  removeEventFilter(this);
}

/**
   Hide event
 */
void city_dialog::hideEvent(QHideEvent *event)
{
  if (event->spontaneous()) {
    return;
  }

  if (pcity) {
    if (!dont_focus) {
      unit_focus_update();
    }
    update_map_canvas_visible();
    pcity = nullptr;
  }
  queen()->mapview_wdg->show_all_fcwidgets();
  king()->menu_bar->minimap_status->setEnabled(true);
}

/**
   Show event
 */
void city_dialog::showEvent(QShowEvent *event)
{
  if (event->spontaneous()) {
    return;
  }

  dont_focus = false;
  if (pcity) {
    unit_focus_set(nullptr);
    update_map_canvas_visible();
    king()->menu_bar->minimap_status->setEnabled(false);
  }
}

/**
   Event filter for catching keybaord events
 */
bool city_dialog::eventFilter(QObject *obj, QEvent *event)
{
  if (obj == this) {
    if (event->type() == QEvent::ShortcutOverride) {
      QKeyEvent *key_event = static_cast<QKeyEvent *>(event);
      if (key_event->key() == Qt::Key_Up) {
        change_production(true);
        event->setAccepted(true);
        return true;
      }
      if (key_event->key() == Qt::Key_Down) {
        change_production(false);
        event->setAccepted(true);
        return true;
      }
    }
  }
  return QObject::eventFilter(obj, event);
}

/**
   City rename dialog input
 */
void city_dialog::city_rename()
{
  hud_input_box *ask;
  const int city_id = pcity->id;

  if (!can_client_issue_orders()) {
    return;
  }

  ask = new hud_input_box(king()->central_wdg);
  ask->set_text_title_definput(_("What should we rename the city to?"),
                               _("Rename City"), city_name_get(pcity));
  ask->setAttribute(Qt::WA_DeleteOnClose);
  connect(ask, &hud_message_box::accepted, this, [=]() {
    struct city *pcity = game_city_by_number(city_id);
    QByteArray ask_bytes;

    if (!pcity) {
      return;
    }

    ask_bytes = ask->input_edit.text().toLocal8Bit();
    ::city_rename(pcity, ask_bytes.data());
  });
  ask->show();
}

/**
   Received signal about changed qcheckbox - allow disbanding city
 */
void city_dialog::disband_state_changed(bool allow_disband)
{
  bv_city_options new_options;

  BV_CLR_ALL(new_options);

  if (allow_disband) {
    BV_SET(new_options, CITYO_DISBAND);
  } else {
    BV_CLR(new_options, CITYO_DISBAND);
  }

  if (!client_is_observer()) {
    dsend_packet_city_options_req(&client.conn, pcity->id, new_options);
  }
}

/**
   Various refresh after getting new info/reply from server
 */
void city_dialog::refresh()
{
  setUpdatesEnabled(false);
  ui.production_combo_p->blockSignals(true);

  if (pcity) {
    update_title();
    update_info_label();
    update_building();
    update_improvements();
    update_units();
    update_disabled();
    ui.icon->set_city(pcity->id);
    ui.upkeep->set_city(pcity->id);
  } else {
    popdown_city_dialog();
    ui.icon->set_city(-1);
    ui.upkeep->set_city(-1);
  }

  ui.production_combo_p->blockSignals(false);
  setUpdatesEnabled(true);

  auto scale = queen()->mapview_wdg->scale();
  ui.middleSpacer->changeSize(scale * get_citydlg_canvas_width(),
                              scale * get_citydlg_canvas_height(),
                              QSizePolicy::Expanding,
                              QSizePolicy::Expanding);

  updateGeometry();
  update();
}

/**
   Updates information label ( food, prod ... surpluses ...)
 */
void city_dialog::update_info_label()
{
  ui.info_icon_label->setCity(pcity);
  ui.info_icon_label->updateText();
}

/**
   Setups whole city dialog, public function
 */
void city_dialog::setup_ui(struct city *qcity)
{
  if (pcity != qcity) {
    if (pcity) {
      refresh_city_mapcanvas(pcity, pcity->tile, true);
    }
    if (qcity) {
      refresh_city_mapcanvas(qcity, qcity->tile, true);
    }
  }

  pcity = qcity;
  ui.production_combo_p->blockSignals(true);
  refresh();
  ui.production_combo_p->blockSignals(false);
}

/**
   Double clicked item in worklist table in production tab
 */
void city_dialog::dbl_click_p(QTableWidgetItem *item)
{
  Q_UNUSED(item)
  struct worklist queue;
  city_get_queue(pcity, &queue);

  if (selected_row_p < 0 || selected_row_p > worklist_length(&queue)) {
    return;
  }

  worklist_remove(&queue, selected_row_p);
  city_set_queue(pcity, &queue);
}

namespace /* anonymous */ {
/**
 * Finds how deeply the unit is nested in transports.
 */
int transport_depth(const unit *unit)
{
  int depth = 0;
  for (auto parent = unit->transporter; parent != nullptr;
       parent = parent->transporter) {
    depth++;
  }
  return depth;
}

/**
 * Comparison function to sort units as shown in the city dialog.
 */
int units_sort(const unit *const *plhs, const unit *const *prhs)
{
  if (plhs == prhs || *plhs == *prhs) {
    return 0;
  }

  auto lhs = *plhs;
  auto rhs = *prhs;

  // Transports are shown before the units they transport.
  if (lhs == rhs->transporter) {
    return false;
  } else if (lhs->transporter == rhs) {
    return true;
  }

  // When one unit is deeper or the two transporters are different, compare
  // the parents instead.
  int lhs_depth = transport_depth(lhs);
  int rhs_depth = transport_depth(rhs);
  if (lhs_depth > rhs_depth) {
    return units_sort(&lhs->transporter, &rhs);
  } else if (lhs_depth < rhs_depth) {
    return units_sort(&lhs, &rhs->transporter);
  } else if (lhs->transporter != rhs->transporter) {
    return units_sort(&lhs->transporter, &rhs->transporter);
  }

  // Put defensive units on the left
  if (lhs->utype->defense_strength != rhs->utype->defense_strength) {
    return rhs->utype->defense_strength - lhs->utype->defense_strength;
  }

  // Put fortified units on the left, then fortifying units, then sentried
  // units.
  for (auto activity :
       {ACTIVITY_FORTIFIED, ACTIVITY_FORTIFYING, ACTIVITY_SENTRY}) {
    if (lhs->activity == activity && rhs->activity != activity) {
      return false;
    } else if (lhs->activity != activity && rhs->activity == activity) {
      return true;
    }
  }

  // Order by unit type
  if (lhs->utype != rhs->utype) {
    return lhs->utype->item_number - rhs->utype->item_number;
  }

  // Then unit id
  return lhs->id - rhs->id;
}
} // anonymous namespace

/**
   Updates layouts for supported and present units in city
 */
void city_dialog::update_units()
{
  struct unit_list *units;
  char buf[256];
  int n;

  if (nullptr != client.conn.playing
      && city_owner(pcity) != client.conn.playing) {
    units = pcity->client.info_units_supported;
  } else {
    units = pcity->units_supported;
  }

  if (nullptr != client.conn.playing
      && city_owner(pcity) != client.conn.playing) {
    units = pcity->client.info_units_present;
  } else {
    units = pcity->tile->units;
  }

  n = unit_list_size(units);
  ui.present_units_list->set_units(units);
  ui.present_units_list->setVisible(n > 0);
  fc_snprintf(buf, sizeof(buf), _("Present units: %d"), n);
  ui.curr_units->setText(QString(buf));
}

void city_dialog::get_city(bool next)
{
  int size, i, j;
  struct city *other_pcity = nullptr;

  if (nullptr == client.conn.playing) {
    return;
  }

  size = city_list_size(client.conn.playing->cities);

  if (size == 1) {
    return;
  }

  for (i = 0; i < size; i++) {
    if (pcity == city_list_get(client.conn.playing->cities, i)) {
      break;
    }
  }

  for (j = 1; j < size; j++) {
    int k = next ? j : -j;
    other_pcity =
        city_list_get(client.conn.playing->cities, (i + k + size) % size);
  }
  queen()->mapview_wdg->center_on_tile(other_pcity->tile);
  setup_ui(other_pcity);
}

/**
   Changes city_dialog to next city after pushing next city button
 */
void city_dialog::next_city() { get_city(true); }

/**
   Changes city_dialog to previous city after pushing prev city button
 */
void city_dialog::prev_city() { get_city(false); }

/**
   Updates building improvement/unit
 */
void city_dialog::update_building()
{
  char buf[32];
  QString str;
  int cost = city_production_build_shield_cost(pcity);

  get_city_dialog_production(pcity, buf, sizeof(buf));
  ui.production_combo_p->setRange(0, cost);
  ui.production_combo_p->set_pixmap(&pcity->production);
  if (pcity->shield_stock >= cost) {
    ui.production_combo_p->setValue(cost);
  } else {
    ui.production_combo_p->setValue(pcity->shield_stock);
  }
  ui.production_combo_p->setAlignment(Qt::AlignCenter);
  str = QString(buf);
  str = str.simplified();

  ui.production_combo_p->setFormat(
      QStringLiteral("(%p%) %2\n%1")
          .arg(city_production_name_translation(pcity), str));

  ui.production_combo_p->updateGeometry();
}

/**
   Updates list of improvements
 */
void city_dialog::update_improvements()
{
  QFont f = QApplication::font();
  QFontMetrics fm(f);
  QString str, tooltip;
  QTableWidgetItem *qitem;
  const QPixmap *sprite = nullptr;
  int h, cost, item, targets_used, col, upkeep;
  struct item items[MAX_NUM_PRODUCTION_TARGETS];
  struct universal targets[MAX_NUM_PRODUCTION_TARGETS];
  struct worklist queue;

  cost = 0;
  upkeep = 0;

  h = fm.height() + 6;
  targets_used = collect_already_built_targets(targets, pcity);
  name_and_sort_items(targets, targets_used, items, false, pcity);

  for (item = 0; item < targets_used; item++) {
    struct universal target = items[item].item;
    fc_assert_action(VUT_IMPROVEMENT == target.kind, continue);
    upkeep += city_improvement_upkeep(pcity, target.value.building);
  }

  city_get_queue(pcity, &queue);

  for (int i = 0; i < worklist_length(&queue); i++) {
    struct universal target = queue.entries[i];

    tooltip = QLatin1String("");

    if (VUT_UTYPE == target.kind) {
      str = utype_values_translation(target.value.utype);
      cost = utype_build_shield_cost(pcity, target.value.utype);
      tooltip = get_tooltip_unit(target.value.utype, true).trimmed();
      sprite = get_unittype_sprite(get_tileset(), target.value.utype,
                                   direction8_invalid());
    } else if (target.kind == VUT_IMPROVEMENT) {
      str = city_improvement_name_translation(pcity, target.value.building);
      sprite = get_building_sprite(tileset, target.value.building);
      tooltip = get_tooltip_improvement(target.value.building, pcity, true)
                    .trimmed();

      if (improvement_has_flag(target.value.building, IF_GOLD)) {
        cost = -1;
      } else {
        cost = impr_build_shield_cost(pcity, target.value.building);
      }
    }

    for (col = 0; col < 3; col++) {
      qitem = new QTableWidgetItem();
      qitem->setToolTip(tooltip);

      switch (col) {
      case 0:
        if (sprite) {
          qitem->setData(Qt::DecorationRole, sprite->scaledToHeight(h));
        }
        break;

      case 1:
        if (str.contains('[') && str.contains(']')) {
          int ii, ij;

          ii = str.lastIndexOf('[');
          ij = str.lastIndexOf(']');
          if (ij > ii) {
            str = str.remove(ii, ij - ii + 1);
          }
        }
        qitem->setText(str);
        break;

      case 2:
        qitem->setTextAlignment(Qt::AlignRight);
        qitem->setText(QString::number(cost));
        break;
      }
    }
  }

  ui.upkeep->refresh();
}

/**
   Shows customized table widget with available items to produce
   Shows default targets in overview city page
 */
void city_dialog::show_targets()
{
  production_widget *pw;
  int when = 1;
  pw = new production_widget(this, pcity, future_targets, when,
                             selected_row_p, show_units, false, show_wonders,
                             show_buildings);
  pw->show();
}

/**
   Shows customized table widget with available items to produce
   Shows customized targets in city production page
 */
void city_dialog::show_targets_worklist()
{
  production_widget *pw;
  int when = 4;
  pw = new production_widget(this, pcity, future_targets, when,
                             selected_row_p, show_units, false, show_wonders,
                             show_buildings);
  pw->show();
}

/**
   Puts city name and people count on title
 */
void city_dialog::update_title()
{
  QString buf;

  // Defeat keyboard shortcut mnemonics
  ui.lcity_name->setText(
      QString(city_name_get(pcity))
          .replace(QLatin1String("&"), QLatin1String("&&")));

  if (city_unhappy(pcity)) {
    // TRANS: city dialog title
    buf = QString(_("%1 - %2 citizens - DISORDER"))
              .arg(city_name_get(pcity),
                   population_to_text(city_population(pcity)));
  } else if (city_celebrating(pcity)) {
    // TRANS: city dialog title
    buf = QString(_("%1 - %2 citizens - celebrating"))
              .arg(city_name_get(pcity),
                   population_to_text(city_population(pcity)));
  } else if (city_happy(pcity)) {
    // TRANS: city dialog title
    buf = QString(_("%1 - %2 citizens - happy"))
              .arg(city_name_get(pcity),
                   population_to_text(city_population(pcity)));
  } else {
    // TRANS: city dialog title
    buf = QString(_("%1 - %2 citizens"))
              .arg(city_name_get(pcity),
                   population_to_text(city_population(pcity)));
  }

  setWindowTitle(buf);
}

/**
   Pop up (or bring to the front) a dialog for the given city.  It may or
   may not be modal.
 */
void real_city_dialog_popup(struct city *pcity)
{
  auto *widget = queen()->city_overlay;
  if (!queen()->city_overlay->isVisible()) {
    top_bar_show_map();
    queen()->mapview_wdg->hide_all_fcwidgets();
  }
  queen()->mapview_wdg->center_on_tile(pcity->tile);

  widget->setup_ui(pcity);
  widget->show();
  widget->resize(queen()->mapview_wdg->size());
}

/**
 * Closes the city overlay.
 */
void popdown_city_dialog()
{
  if (queen()) {
    queen()->city_overlay->hide();
  }
}

/**
   Refresh (update) all data for the given city's dialog.
 */
void real_city_dialog_refresh(struct city *pcity)
{
  if (city_dialog_is_open(pcity)) {
    queen()->city_overlay->refresh();
  }
}

/**
   Updates city font
 */
void city_font_update()
{
  QList<QLabel *> l;

  l = queen()->city_overlay->findChildren<QLabel *>();

  auto f = fcFont::instance()->getFont(fonts::notify_label);

  for (auto i : qAsConst(l)) {
    if (i->property(fonts::notify_label).isValid()) {
      i->setFont(f);
    }
  }
}

/**
   Update city dialogs when the given unit's status changes.  This
   typically means updating both the unit's home city (if any) and the
   city in which it is present (if any).
 */
void refresh_unit_city_dialogs(struct unit *punit)
{
  struct city *pcity_sup, *pcity_pre;

  pcity_sup = game_city_by_number(punit->homecity);
  pcity_pre = tile_city(punit->tile);

  real_city_dialog_refresh(pcity_sup);
  real_city_dialog_refresh(pcity_pre);
}

struct city *is_any_city_dialog_open()
{
  // some checks not to iterate cities
  if (!queen()->city_overlay->isVisible()) {
    return nullptr;
  }
  if (client_is_global_observer() || client_is_observer()) {
    return nullptr;
  }

  city_list_iterate(client.conn.playing->cities, pcity)
  {
    if (city_dialog_is_open(pcity)) {
      return pcity;
    }
  }
  city_list_iterate_end;
  return nullptr;
}

/**
   Return whether the dialog for the given city is open.
 */
bool city_dialog_is_open(struct city *pcity)
{
  return queen()->city_overlay->pcity == pcity
         && queen()->city_overlay->isVisible();
}

/**
   City item delegate constructor
 */
city_production_delegate::city_production_delegate(QPoint sh,
                                                   QObject *parent,
                                                   struct city *city)
    : QItemDelegate(parent), pd(sh)
{
  item_height = sh.y();
  pcity = city;
}

/**
   City item delgate paint event
 */
void city_production_delegate::paint(QPainter *painter,
                                     const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
  struct universal *target;
  QString name;
  QVariant qvar;
  QPixmap pix_scaled;
  QRect rect1;
  QRect rect2;
  const QPixmap *sprite;
  bool useless = false;
  bool is_coinage = false;
  bool is_neutral = false;
  bool is_sea = false;
  bool is_flying = false;
  bool is_unit = true;
  QPixmap pix_dec(option.rect.width(), option.rect.height());
  QStyleOptionViewItem opt;
  QIcon icon = qApp->style()->standardIcon(QStyle::SP_DialogCancelButton);
  struct unit_class *pclass;

  if (!option.rect.isValid()) {
    return;
  }

  qvar = index.data();

  if (!qvar.isValid()) {
    return;
  }

  target = reinterpret_cast<universal *>(qvar.value<void *>());

  if (target == nullptr) {
    return;
    /*free_sprite = new QPixmap;
    *free_sprite = icon.pixmap(100, 100);
    sprite = free_sprite;
    name = _("Cancel");
    is_unit = false;*/
  } else if (VUT_UTYPE == target->kind) {
    name = utype_name_translation(target->value.utype);
    is_neutral = utype_has_flag(target->value.utype, UTYF_CIVILIAN);
    pclass = utype_class(target->value.utype);
    if (!uclass_has_flag(pclass, UCF_TERRAIN_DEFENSE)
        && !utype_can_do_action_result(target->value.utype, ACTRES_FORTIFY)
        && !uclass_has_flag(pclass, UCF_ZOC)) {
      is_sea = true;
    }

    if ((utype_fuel(target->value.utype)
         && !uclass_has_flag(pclass, UCF_TERRAIN_DEFENSE)
         && !utype_can_do_action_result(target->value.utype, ACTRES_PILLAGE)
         && !utype_can_do_action_result(target->value.utype, ACTRES_FORTIFY)
         && !uclass_has_flag(pclass, UCF_ZOC))
        /* FIXME: Assumed to be flying since only missiles can do suicide
         * attacks in classic-like rulesets. This isn't true for all
         * rulesets. Not a high priority to fix since all is_flying and
         * is_sea is used for is to set a color. */
        || utype_is_consumed_by_action_result(ACTRES_ATTACK,
                                              target->value.utype)) {
      if (is_sea) {
        is_sea = false;
      }
      is_flying = true;
    }

    sprite = get_unittype_sprite(get_tileset(), target->value.utype,
                                 direction8_invalid());
  } else {
    is_unit = false;
    name = improvement_name_translation(target->value.building);
    sprite = get_building_sprite(tileset, target->value.building);
    useless = is_improvement_redundant(pcity, target->value.building);
    is_coinage = improvement_has_flag(target->value.building, IF_GOLD);
  }

  if (sprite != nullptr) {
    pix_scaled =
        sprite->scaledToHeight(item_height - 2, Qt::SmoothTransformation);

    if (useless) {
      pixmap_put_x(&pix_scaled);
    }
  }

  opt = QItemDelegate::setOptions(index, option);
  painter->save();
  opt.displayAlignment = Qt::AlignLeft;
  opt.textElideMode = Qt::ElideMiddle;
  QItemDelegate::drawBackground(painter, opt, index);
  rect1 = option.rect;
  rect1.setWidth(pix_scaled.width() + 4);
  rect2 = option.rect;
  rect2.setLeft(option.rect.left() + rect1.width());
  rect2.setTop(rect2.top()
               + (rect2.height() - painter->fontMetrics().height()) / 2);
  QItemDelegate::drawDisplay(painter, opt, rect2, name);

  if (is_unit) {
    if (is_sea) {
      pix_dec.fill(QColor(0, 0, 255, 80));
    } else if (is_flying) {
      pix_dec.fill(QColor(220, 0, 0, 80));
    } else if (is_neutral) {
      pix_dec.fill(QColor(0, 120, 0, 40));
    } else {
      pix_dec.fill(QColor(0, 0, 150, 40));
    }

    QItemDelegate::drawDecoration(painter, option, option.rect, pix_dec);
  }

  if (is_coinage) {
    pix_dec.fill(QColor(255, 255, 0, 70));
    QItemDelegate::drawDecoration(painter, option, option.rect, pix_dec);
  }

  if (!pix_scaled.isNull()) {
    QItemDelegate::drawDecoration(painter, opt, rect1, pix_scaled);
  }

  drawFocus(painter, opt, option.rect);

  painter->restore();
}

/**
   Draws focus for given item
 */
void city_production_delegate::drawFocus(QPainter *painter,
                                         const QStyleOptionViewItem &option,
                                         const QRect &rect) const
{
  QPixmap pix(option.rect.width(), option.rect.height());

  if ((option.state & QStyle::State_MouseOver) == 0 || !rect.isValid()) {
    return;
  }

  pix.fill(QColor(50, 50, 50, 50));
  QItemDelegate::drawDecoration(painter, option, option.rect, pix);
}

/**
   Size hint for city item delegate
 */
QSize city_production_delegate::sizeHint(const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const
{
  Q_UNUSED(option)
  Q_UNUSED(index)
  QSize s;

  s.setWidth(pd.x());
  s.setHeight(pd.y());
  return s;
}

/**
   Production item constructor
 */
production_item::production_item(struct universal *ptarget, QObject *parent)
    : QObject()
{
  setParent(parent);
  target = ptarget;
}

/**
   Production item destructor
 */
production_item::~production_item()
{
  // allocated as renegade in model
  delete target;
}

/**
   Returns stored data
 */
QVariant production_item::data() const
{
  return QVariant::fromValue((void *) target);
}

/**
   Constructor for city production model
 */
city_production_model::city_production_model(struct city *pcity, bool f,
                                             bool su, bool sw, bool sb,
                                             QObject *parent)
    : QAbstractListModel(parent)
{
  show_units = su;
  show_wonders = sw;
  show_buildings = sb;
  mcity = pcity;
  future_t = f;
  populate();
}

/**
   Destructor for city production model
 */
city_production_model::~city_production_model()
{
  qDeleteAll(city_target_list);
  city_target_list.clear();
}

/**
   Returns data from model
 */
QVariant city_production_model::data(const QModelIndex &index,
                                     int role) const
{
  if (!index.isValid()) {
    return QVariant();
  }

  if (index.row() >= 0 && index.row() < rowCount() && index.column() >= 0
      && index.column() < columnCount()
      && (index.column() + index.row() * 3 < city_target_list.count())) {
    int r, c, t, new_index;
    r = index.row();
    c = index.column();
    t = r * 3 + c;
    new_index = t / 3 + rowCount() * c;
    // Exception, shift whole column
    if ((c == 2) && city_target_list.count() % 3 == 1) {
      new_index = t / 3 + rowCount() * c - 1;
    }
    if (role == Qt::ToolTipRole) {
      return get_tooltip(city_target_list[new_index]->data());
    }

    return city_target_list[new_index]->data();
  }

  return QVariant();
}

/**
   Fills model with data
 */
void city_production_model::populate()
{
  production_item *pi;
  struct universal targets[MAX_NUM_PRODUCTION_TARGETS];
  struct item items[MAX_NUM_PRODUCTION_TARGETS];
  struct universal *renegade, *renegate;
  int item, targets_used;
  QString str;
  auto f = fcFont::instance()->getFont(fonts::default_font);
  QFontMetrics fm(f);

  sh.setY(fm.height() * 2);
  sh.setX(0);

  qDeleteAll(city_target_list);
  city_target_list.clear();

  targets_used =
      collect_eventually_buildable_targets(targets, mcity, future_t);
  name_and_sort_items(targets, targets_used, items, false, mcity);

  for (item = 0; item < targets_used; item++) {
    if (future_t || can_city_build_now(mcity, &items[item].item)) {
      renegade = new universal(items[item].item);

      // renagade deleted in production_item destructor
      if (VUT_UTYPE == renegade->kind) {
        str = utype_name_translation(renegade->value.utype);
        sh.setX(qMax(sh.x(), fm.horizontalAdvance(str)));

        if (show_units) {
          pi = new production_item(renegade, this);
          city_target_list << pi;
        }
      } else {
        str = improvement_name_translation(renegade->value.building);
        sh.setX(qMax(sh.x(), fm.horizontalAdvance(str)));

        if ((is_wonder(renegade->value.building) && show_wonders)
            || (is_improvement(renegade->value.building) && show_buildings)
            || (improvement_has_flag(renegade->value.building, IF_GOLD))
            || (is_special_improvement(renegade->value.building)
                && show_buildings)) {
          pi = new production_item(renegade, this);
          city_target_list << pi;
        }
      }
    }
  }

  renegate = nullptr;
  pi = new production_item(renegate, this);
  city_target_list << pi;
  sh.setX(2 * sh.y() + sh.x());
  sh.setX(qMin(sh.x(), 250));
}

/**
   Constructor for production widget
   future - show future targets
   show_units - if to show units
   when - where to insert
   curr - current index to insert
   buy - buy if possible
 */
production_widget::production_widget(QWidget *parent, struct city *pcity,
                                     bool future, int when, int curr,
                                     bool show_units, bool buy,
                                     bool show_wonders, bool show_buildings)
    : QTableView()
{
  Q_UNUSED(parent)
  QPoint pos, sh;
  auto temp = QGuiApplication::screens();
  int desk_width = temp[0]->availableGeometry().width();
  int desk_height = temp[0]->availableGeometry().height();
  fc_tt = new fc_tooltip(this);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowFlags(Qt::Popup);
  verticalHeader()->setVisible(false);
  horizontalHeader()->setVisible(false);
  setProperty("showGrid", false);
  curr_selection = curr;
  pw_city = pcity;
  buy_it = buy;
  when_change = when;
  list_model = new city_production_model(pw_city, future, show_units,
                                         show_wonders, show_buildings, this);
  sh = list_model->sh;
  c_p_d = new city_production_delegate(sh, this, pw_city);
  setItemDelegate(c_p_d);
  setModel(list_model);
  viewport()->installEventFilter(fc_tt);
  installEventFilter(this);
  connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &production_widget::prod_selected);
  resizeRowsToContents();
  resizeColumnsToContents();
  setFixedWidth(3 * sh.x() + 6);
  setFixedHeight(list_model->rowCount() * sh.y() + 6);

  if (width() > desk_width) {
    setFixedWidth(desk_width);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  } else {
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  }

  if (height() > desk_height) {
    setFixedHeight(desk_height);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  } else {
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  }

  pos = QCursor::pos();

  if (pos.x() + width() > desk_width) {
    pos.setX(desk_width - width());
  } else if (pos.x() - width() < 0) {
    pos.setX(0);
  }

  if (pos.y() + height() > desk_height) {
    pos.setY(desk_height - height());
  } else if (pos.y() - height() < 0) {
    pos.setY(0);
  }

  move(pos);
  setMouseTracking(true);
  setFocus();
}

/**
   Mouse press event for production widget
 */
void production_widget::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::RightButton) {
    close();
    return;
  }

  QAbstractItemView::mousePressEvent(event);
}

/**
   Event filter for production widget
 */
bool production_widget::eventFilter(QObject *obj, QEvent *ev)
{
  QRect pw_rect;
  QPoint br;

  if (obj != this) {
    return false;
  }

  if (ev->type() == QEvent::MouseButtonPress) {
    pw_rect.setTopLeft(pos());
    br.setX(pos().x() + width());
    br.setY(pos().y() + height());
    pw_rect.setBottomRight(br);

    if (!pw_rect.contains(QCursor::pos())) {
      close();
    }
  }

  return false;
}

/**
   Changed selection in production widget
 */
void production_widget::prod_selected(const QItemSelection &sl,
                                      const QItemSelection &ds)
{
  Q_UNUSED(ds)
  Q_UNUSED(sl)
  QModelIndexList indexes = selectionModel()->selectedIndexes();
  QModelIndex index;
  QVariant qvar;
  struct worklist queue;
  struct universal *target;

  if (indexes.isEmpty() || client_is_observer()) {
    return;
  }
  index = indexes.at(0);
  qvar = index.data(Qt::UserRole);
  if (!qvar.isValid()) {
    return;
  }
  target = reinterpret_cast<universal *>(qvar.value<void *>());
  if (target != nullptr) {
    city_get_queue(pw_city, &queue);
    switch (when_change) {
    case 0: // Change current target
      city_change_production(pw_city, target);
      if (city_can_buy(pw_city) && buy_it) {
        city_buy_production(pw_city);
      }
      break;

    case 1: /* Change current (selected on list)*/
      if (curr_selection < 0 || curr_selection > worklist_length(&queue)) {
        city_change_production(pw_city, target);
      } else {
        worklist_remove(&queue, curr_selection);
        worklist_insert(&queue, target, curr_selection);
        city_set_queue(pw_city, &queue);
      }
      break;

    case 2: // Insert before
      if (curr_selection < 0 || curr_selection > worklist_length(&queue)) {
        curr_selection = 0;
      }
      curr_selection--;
      curr_selection = qMax(0, curr_selection);
      worklist_insert(&queue, target, curr_selection);
      city_set_queue(pw_city, &queue);
      break;

    case 3: // Insert after
      if (curr_selection < 0 || curr_selection > worklist_length(&queue)) {
        city_queue_insert(pw_city, -1, target);
        break;
      }
      curr_selection++;
      worklist_insert(&queue, target, curr_selection);
      city_set_queue(pw_city, &queue);
      break;

    case 4: // Add last
      city_queue_insert(pw_city, -1, target);
      break;

    default:
      break;
    }
  }
  close();
  destroy();
}

/**
   Destructor for production widget
 */
production_widget::~production_widget()
{
  delete c_p_d;
  delete list_model;
  viewport()->removeEventFilter(fc_tt);
  removeEventFilter(this);
}
