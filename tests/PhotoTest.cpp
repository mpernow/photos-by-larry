#include "core/Photo.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace
{
EditParameters makeNonIdentityParams()
{
    EditParameters params;
    params.brightness = 25.0;
    params.contrast = 1.3;
    params.temperature = -10.0;
    params.saturation = 15.0;
    return params;
}
} // namespace

TEST(PhotoTest, SidecarPathAppendsSuffix)
{
    Photo photo("/some/dir/IMG_0001.jpg");
    EXPECT_EQ(photo.sidecarPath(), QString("/some/dir/IMG_0001.jpg.larryedit.json"));
}

TEST(PhotoTest, FileNameStripsDirectory)
{
    Photo photo("/some/dir/IMG_0001.jpg");
    EXPECT_EQ(photo.fileName(), QString("IMG_0001.jpg"));
}

TEST(PhotoTest, NewPhotoWithNoSidecarHasDefaultState)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("IMG_0001.jpg"); // deliberately never created

    Photo photo(path);
    EXPECT_TRUE(photo.editParameters().isIdentity());
    EXPECT_FALSE(photo.isFavorite());
    EXPECT_FALSE(photo.hasUnsavedEdits());
}

TEST(PhotoTest, SettingParamsOrFavoriteMarksDirty)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    Photo photo(dir.filePath("IMG_0001.jpg"));

    photo.setEditParameters(makeNonIdentityParams());
    EXPECT_TRUE(photo.hasUnsavedEdits());
}

TEST(PhotoTest, SettingSameFavoriteTwiceDoesNotStayDirty)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    Photo photo(dir.filePath("IMG_0001.jpg"));

    ASSERT_FALSE(photo.isFavorite());
    photo.setFavorite(false); // already false - should be a no-op
    EXPECT_FALSE(photo.hasUnsavedEdits());
}

TEST(PhotoTest, SaveSidecarIsNoOpWhenNotDirty)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    Photo photo(dir.filePath("IMG_0001.jpg"));

    photo.saveSidecar();
    EXPECT_FALSE(QFile::exists(photo.sidecarPath()));
}

TEST(PhotoTest, SaveThenReloadRoundTripsEditParametersAndFavorite)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("IMG_0001.jpg");

    const EditParameters params = makeNonIdentityParams();
    {
        Photo photo(path);
        photo.setEditParameters(params);
        photo.setFavorite(true);
        photo.saveSidecar();
        EXPECT_FALSE(photo.hasUnsavedEdits()); // saving clears the dirty flag
    }

    ASSERT_TRUE(QFile::exists(path + ".larryedit.json"));

    // A fresh Photo instance for the same path simulates reopening the
    // directory in a new session - it should pick the sidecar back up.
    Photo reloaded(path);
    EXPECT_EQ(reloaded.editParameters(), params);
    EXPECT_TRUE(reloaded.isFavorite());
}

TEST(PhotoTest, SidecarWithoutFavoriteKeyDefaultsToNotFavorite)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("IMG_0001.jpg");

    // Hand-written sidecar predating the "favorite" field, to protect the
    // documented backward-compatibility guarantee: a missing key defaults
    // to false rather than failing to parse.
    QFile sidecar(path + ".larryedit.json");
    ASSERT_TRUE(sidecar.open(QIODevice::WriteOnly));
    sidecar.write(R"({"brightness": 10})");
    sidecar.close();

    Photo photo(path);
    EXPECT_FALSE(photo.isFavorite());
    EXPECT_DOUBLE_EQ(photo.editParameters().brightness, 10.0);
}
