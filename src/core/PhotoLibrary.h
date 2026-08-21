#pragma once

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

class Photo;

// Owns the set of photos found in a single opened directory. This is the
// model layer's root object: the UI drives it by calling openDirectory(),
// and reacts to libraryChanged() to refresh whatever it's showing.
class PhotoLibrary : public QObject
{
    Q_OBJECT
public:
    explicit PhotoLibrary(QObject *parent = nullptr);
    ~PhotoLibrary() override;

    void openDirectory(const QString &directoryPath);

    const QString &directoryPath() const { return m_directoryPath; }
    int count() const;
    Photo *photoAt(int index) const;

signals:
    void libraryChanged();

private:
    QString m_directoryPath;
    std::vector<std::unique_ptr<Photo>> m_photos;
};
