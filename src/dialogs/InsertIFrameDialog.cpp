#include "InsertIFrameDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QUrl>
#include <QUrlQuery>

// Rewrites well-known "watch" links to their embeddable form.
// Most other sites (Google-Suche, viele News-Portale usw.) senden aktiv den
// Header X-Frame-Options/CSP frame-ancestors und verweigern das Einbetten
// per iframe grundsätzlich – das lässt sich technisch nicht umgehen.
static QString normalizeIFrameUrl(const QString& raw) {
    QString url = raw.trimmed();
    QUrl u(url, QUrl::TolerantMode);
    if (!u.isValid()) return url;

    QString host = u.host().toLower();
    QString path = u.path();

    if (host == "youtube.com" || host == "www.youtube.com" || host == "m.youtube.com") {
        if (path == "/watch") {
            QString id = QUrlQuery(u).queryItemValue("v");
            if (!id.isEmpty())
                return "https://www.youtube.com/embed/" + id;
        } else if (path.startsWith("/shorts/")) {
            QString id = path.mid(QStringLiteral("/shorts/").size());
            if (!id.isEmpty())
                return "https://www.youtube.com/embed/" + id;
        }
    } else if (host == "youtu.be") {
        QString id = path.startsWith('/') ? path.mid(1) : path;
        if (!id.isEmpty())
            return "https://www.youtube.com/embed/" + id;
    }

    return url;
}

InsertIFrameDialog::InsertIFrameDialog(QWidget* parent, const QString& initialUrl)
    : QDialog(parent) {
    setWindowTitle(initialUrl.isEmpty() ? "Insert iFrame" : "Edit iFrame");
    setMinimumWidth(440);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* hdr = new QLabel("Web Link (URL)", this);
    hdr->setStyleSheet("font-size:14px;font-weight:bold;color:#111827;");
    layout->addWidget(hdr);

    auto* hint = new QLabel(
        "YouTube links are automatically converted to the embeddable /embed/ form.", this);
    hint->setStyleSheet("color:#6b7280; font-size:11px;");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_edit = new QLineEdit(this);
    m_edit->setText(initialUrl);
    m_edit->setPlaceholderText("https://…");
    m_edit->setStyleSheet("background:#ffffff; color:#111827;");
    layout->addWidget(m_edit);

    auto* warn = new QLabel(
        "⚠ Some sites (e.g. google.com and many others) block embedding "
        "via iframe entirely (X-Frame-Options) – this comes from the target site itself "
        "and cannot be worked around. Affected sites will show an error instead of "
        "their content.", this);
    warn->setStyleSheet("color:#b45309; font-size:11px;");
    warn->setWordWrap(true);
    layout->addWidget(warn);

    auto* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bbox->button(QDialogButtonBox::Ok)->setText(initialUrl.isEmpty() ? "Insert" : "Apply");
    layout->addWidget(bbox);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_edit->setFocus();
}

QString InsertIFrameDialog::url() const {
    return normalizeIFrameUrl(m_edit->text());
}
