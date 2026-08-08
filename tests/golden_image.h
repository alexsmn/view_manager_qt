#ifndef VIEW_MANAGER_QT_TESTS_GOLDEN_IMAGE_H_
#define VIEW_MANAGER_QT_TESTS_GOLDEN_IMAGE_H_

#include <QFile>
#include <QImage>
#include <QString>

#include <functional>

namespace view_manager_qt_test {

// Outcome of reading a golden image from disk.
//
// `kAbsent` and `kUnreadable` are deliberately distinct. A missing golden is
// the first-run case a test may regenerate; a golden that exists but does not
// decode means the baseline is damaged — a truncated file, or a build without
// the codec — and regenerating it there would silently replace the reviewed
// baseline with whatever the current code renders.
enum class GoldenLoadResult {
  kLoaded,
  kAbsent,
  kUnreadable,
};

// Reads the golden image at `path` into `image`. `image` is left untouched
// unless the result is `kLoaded`.
inline GoldenLoadResult LoadGoldenImage(const QString& path, QImage& image) {
  if (!QFile::exists(path)) {
    return GoldenLoadResult::kAbsent;
  }

  QImage loaded;
  if (!loaded.load(path)) {
    return GoldenLoadResult::kUnreadable;
  }

  image = std::move(loaded);
  return GoldenLoadResult::kLoaded;
}

// Encodes an image to a path. Injectable into SaveGoldenImage so a test can
// force the failure this API exists to contain.
using GoldenImageWriter = std::function<bool(const QImage&, const QString&)>;

// Returns `path` with `infix` inserted before its extension, so a scratch file
// beside a golden keeps the suffix. QImage::save infers the format from the
// suffix, so a plain "<path>.tmp" would be unencodable.
inline QString InsertGoldenPathInfix(const QString& path,
                                     const QString& infix) {
  const int slash = path.lastIndexOf(u'/');
  const int dot = path.lastIndexOf(u'.');
  if (dot <= slash + 1) {
    return path + infix;
  }
  return path.left(dot) + infix + path.mid(dot);
}

// Writes `image` to `path` without putting an existing file at risk: the
// encode goes to a temporary sibling, and only a fully written file is moved
// into place. Returns false, leaving any existing file as it was, if either
// step fails.
//
// Writing straight onto the golden is what this replaces. QImageWriter opens
// and truncates its destination before it discovers it has no codec for the
// format, so a failed encode over a tracked golden leaves a 0-byte file where
// the baseline was — and the next run reads that as "no golden yet" and
// regenerates one from the current render.
inline bool SaveGoldenImage(const QImage& image,
                            const QString& path,
                            const GoldenImageWriter& writer) {
  const QString temp_path = InsertGoldenPathInfix(path, ".tmp");
  QFile::remove(temp_path);
  if (!writer(image, temp_path)) {
    QFile::remove(temp_path);
    return false;
  }

  // QFile::rename does not overwrite, so an existing golden is moved aside
  // first and put back if the move into place fails.
  const QString backup_path = InsertGoldenPathInfix(path, ".bak");
  QFile::remove(backup_path);
  const bool had_existing = QFile::exists(path);
  if (had_existing && !QFile::rename(path, backup_path)) {
    QFile::remove(temp_path);
    return false;
  }

  if (!QFile::rename(temp_path, path)) {
    if (had_existing) {
      QFile::rename(backup_path, path);
    }
    QFile::remove(temp_path);
    return false;
  }

  QFile::remove(backup_path);
  return true;
}

// Writes `image` to `path` as above, encoding with QImage::save.
inline bool SaveGoldenImage(const QImage& image, const QString& path) {
  return SaveGoldenImage(image, path,
                         [](const QImage& to_write, const QString& to) {
                           return to_write.save(to);
                         });
}

}  // namespace view_manager_qt_test

#endif  // VIEW_MANAGER_QT_TESTS_GOLDEN_IMAGE_H_
