#include "gui/pages/env_vars_page.h"

#include "gui/theme.h"
#include "gui/widgets/icons.h"
#include "gui/widgets/table_factory.h"
#include "gui/widgets/tag_badge.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRegularExpression>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace dm {

QString EnvVarsPage::classify(const QString& name, const QString& value)
{
    static const QRegularExpression secret(
        "KEY|TOKEN|SECRET|PASSWORD|PASSWD|CREDENTIAL|AUTH|PRIVATE",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression svc(
        "HEADROOM|OMNIROUTE|OLLAMA|DOCKER|_PORT|_HOST|_URL|BASE_URL|ENDPOINT",
        QRegularExpression::CaseInsensitiveOption);
    if (secret.match(name).hasMatch())
        return "secret";
    if (name.compare("PATH", Qt::CaseInsensitive) == 0 || name.endsWith("PATH")
        || value.contains(QRegularExpression("^[A-Za-z]:[\\\\/]"))
        || (value.contains(';') && value.contains('\\')))
        return "path";
    if (value.startsWith("http://") || value.startsWith("https://")
        || svc.match(name).hasMatch())
        return "service";
    return "normal";
}

EnvVarsPage::EnvVarsPage(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(Metric::OuterMargin, 14, Metric::OuterMargin, Metric::OuterMargin);
    lay->setSpacing(Metric::Gap);

    auto* title = new QLabel("Env Vars");
    title->setObjectName("pageTitle");
    title->setFont(uiFont(16, QFont::Bold, true));
    auto* sub = new QLabel("Relevant environment variables. Secret values stay masked.");
    sub->setObjectName("pageSubtitle");
    lay->addWidget(title);
    lay->addWidget(sub);

    m_count = new QLabel;
    m_count->setStyleSheet(QString("color:%1;").arg(Color::Muted));
    lay->addWidget(m_count);

    m_table = makeListTable({"Name", "Value", "Type", ""}, 0, Qt::ElideRight);
    auto* hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(1, QHeaderView::Stretch);
    hh->setSectionResizeMode(2, QHeaderView::Fixed);
    hh->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 100);
    m_table->setColumnWidth(3, 44);
    lay->addWidget(m_table, 1);
}

void EnvVarsPage::setInventory(const EnvironmentInventory& inv)
{
    m_inv = inv;
    rebuild();
}

void EnvVarsPage::setSearchFilter(const QString& text)
{
    m_search = text;
    rebuild();
}

void EnvVarsPage::rebuild()
{
    QStringList names = m_inv.env.keys();
    names.sort(Qt::CaseInsensitive);

    QStringList shown;
    for (const QString& n : names)
        if (m_search.isEmpty() || n.contains(m_search, Qt::CaseInsensitive))
            shown << n;

    m_table->clearContents();
    m_table->setRowCount(shown.size());
    for (int i = 0; i < shown.size(); ++i) {
        const QString name = shown.at(i);
        const QString value = m_inv.env.value(name);
        const QString type = classify(name, value);

        auto* n = new QTableWidgetItem(name);
        n->setFont(uiFont(9, QFont::DemiBold));
        m_table->setItem(i, 0, n);

        auto* v = new QTableWidgetItem(value);
        v->setFont(monoFont(9));
        v->setForeground(QColor(type == "secret" ? Color::Muted : Color::TextSecondary));
        v->setToolTip(type == "secret" ? QStringLiteral("masked") : value);
        m_table->setItem(i, 1, v);

        auto* badge = new TagBadge;
        const char* col = type == "secret" ? Color::Danger
                          : type == "path" ? Color::Warning
                          : type == "service" ? Color::Primary
                                              : Color::TextSecondary;
        badge->setTag(type, col);
        m_table->setCellWidget(i, 2, TagBadge::cell(badge));

        auto* copy = new QToolButton;
        copy->setObjectName("iconBtn");
        copy->setIcon(icons::icon("copy", QColor(Color::Muted), 13));
        copy->setCursor(Qt::PointingHandCursor);
        if (type == "secret") {
            copy->setEnabled(false);
            copy->setToolTip("Copy disabled for secrets");
        } else {
            copy->setToolTip("Copy value");
            connect(copy, &QToolButton::clicked, this,
                    [value] { QApplication::clipboard()->setText(value); });
        }
        m_table->setCellWidget(i, 3, copy);
    }
    m_count->setText(QString("%1 of %2").arg(shown.size()).arg(m_inv.env.size()));
}

} // namespace dm
