#ifndef T55XXRECOVERY_H
#define T55XXRECOVERY_H

#include <QRegularExpression>
#include <QString>

namespace T55xxRecovery
{
inline QString cleanOutput(QString output)
{
    output.remove(QRegularExpression("\x1b\\[[0-9;]*m"));
    return output;
}

inline QString confirmedPassword(const QString& output)
{
    const QString clean = cleanOutput(output);
    const QRegularExpression found(
        "^\\s*\\[\\+\\]\\s+found valid password:\\s*\\[\\s*([0-9A-Fa-f]{8})\\s*\\]\\s*$",
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    return found.match(clean).captured(1).toUpper();
}

inline bool passwordCleared(const QString& output)
{
    const QString clean = cleanOutput(output);
    return QRegularExpression("Block0\\.{2,}\\s*[0-9A-Fa-f]{8}\\s*\\(auto detect\\)").match(clean).hasMatch()
        && QRegularExpression("Password set\\.{2,}\\s*No\\b").match(clean).hasMatch()
        && !QRegularExpression("Password set\\.{2,}\\s*Yes\\b").match(clean).hasMatch();
}
}

#endif
