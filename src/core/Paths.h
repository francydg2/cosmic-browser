#pragma once

#include <QString>

/// XDG-conformant directories for Cosmic (spec §55):
///   ~/.config/cosmic
///   ~/.local/share/cosmic
///   ~/.cache/cosmic
///
/// Must be called after QCoreApplication exists (paths depend on the
/// application name, which main() sets to "cosmic" with no organization,
/// so no extra path segment is added).
namespace Paths {

QString configDir();
QString dataDir();
QString cacheDir();

/// Creates the three directories if missing. Returns false if any could
/// not be created (logged).
bool ensureDirs();

/// One-time migration from the Nebula alpha profile: copies missing
/// files (settings, session, users, vault, bookmarks, UI layout) and
/// profile storage from the legacy "nebula" XDG directories into the
/// current ones. Never overwrites existing files, never deletes the
/// source. Returns true when there was nothing to do or everything
/// was copied.
bool migrateLegacyProfile();

} // namespace Paths
