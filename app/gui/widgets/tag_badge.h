#pragma once
#include <QLabel>

namespace dm {

// Small rounded pill: tinted background + colored text. For type/category tags
// (User / Linked / System, npm / pip, Secret / Path / ...).
class TagBadge : public QLabel {
    Q_OBJECT
public:
    explicit TagBadge(QWidget* parent = nullptr);
    void setTag(const QString& text, const QString& colorHex);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

    // convenience: known kinds -> text + color
    static TagBadge* forSkillType(const QString& classification, QWidget* parent = nullptr);

    // wrap a badge in a left-aligned cell container (for QTableWidget::setCellWidget)
    static QWidget* cell(TagBadge* badge);
};

} // namespace dm
