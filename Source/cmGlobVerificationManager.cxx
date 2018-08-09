/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmGlobVerificationManager.h"

#include "cmsys/FStream.hxx"
#include <sstream>

#include "cmGeneratedFileStream.h"
#include "cmListFileCache.h"
#include "cmSystemTools.h"
#include "cmVersion.h"
#include "cmake.h"

bool cmGlobVerificationManager::SaveVerificationScript(const std::string& path)
{
  if (this->Cache.empty()) {
    return true;
  }

  std::string scriptFile = path;
  scriptFile += cmake::GetCMakeFilesDirectory();
  std::string stampFile = scriptFile;
  cmSystemTools::MakeDirectory(scriptFile);
  scriptFile += "/VerifyGlobs.cmake";
  stampFile += "/cmake.verify_globs";
  cmGeneratedFileStream verifyScriptFile(scriptFile.c_str());
  verifyScriptFile.SetCopyIfDifferent(true);
  if (!verifyScriptFile) {
    cmSystemTools::Error("Unable to open verification script file for save. ",
                         scriptFile.c_str());
    cmSystemTools::ReportLastSystemError("");
    return false;
  }

  verifyScriptFile << std::boolalpha;
  verifyScriptFile << "# CMAKE generated file: DO NOT EDIT!\n"
                   << "# Generated by CMake Version "
                   << cmVersion::GetMajorVersion() << "."
                   << cmVersion::GetMinorVersion() << "\n";

  for (auto const& i : this->Cache) {
    CacheEntryKey k = std::get<0>(i);
    CacheEntryValue v = std::get<1>(i);

    if (!v.Initialized) {
      continue;
    }

    verifyScriptFile << "\n";

    for (auto const& bt : v.Backtraces) {
      verifyScriptFile << "# " << std::get<0>(bt);
      std::get<1>(bt).PrintTitle(verifyScriptFile);
      verifyScriptFile << "\n";
    }

    k.PrintGlobCommand(verifyScriptFile, "NEW_GLOB");
    verifyScriptFile << "\n";

    verifyScriptFile << "set(OLD_GLOB\n";
    for (const std::string& file : v.Files) {
      verifyScriptFile << "  \"" << file << "\"\n";
    }
    verifyScriptFile << "  )\n";

    verifyScriptFile << "if(NOT \"${NEW_GLOB}\" STREQUAL \"${OLD_GLOB}\")\n"
                     << "  message(\"-- GLOB mismatch!\")\n"
                     << "  file(TOUCH_NOCREATE \"" << stampFile << "\")\n"
                     << "endif()\n";
  }
  verifyScriptFile.Close();

  cmsys::ofstream verifyStampFile(stampFile.c_str());
  if (!verifyStampFile) {
    cmSystemTools::Error("Unable to open verification stamp file for write. ",
                         stampFile.c_str());
    return false;
  }
  verifyStampFile << "# This file is generated by CMake for checking of the "
                     "VerifyGlobs.cmake file\n";
  this->VerifyScript = scriptFile;
  this->VerifyStamp = stampFile;
  return true;
}

bool cmGlobVerificationManager::DoWriteVerifyTarget() const
{
  return !this->VerifyScript.empty() && !this->VerifyStamp.empty();
}

bool cmGlobVerificationManager::CacheEntryKey::operator<(
  const CacheEntryKey& r) const
{
  if (this->Recurse < r.Recurse) {
    return true;
  }
  if (this->Recurse > r.Recurse) {
    return false;
  }
  if (this->ListDirectories < r.ListDirectories) {
    return true;
  }
  if (this->ListDirectories > r.ListDirectories) {
    return false;
  }
  if (this->FollowSymlinks < r.FollowSymlinks) {
    return true;
  }
  if (this->FollowSymlinks > r.FollowSymlinks) {
    return false;
  }
  if (this->Relative < r.Relative) {
    return true;
  }
  if (this->Relative > r.Relative) {
    return false;
  }
  if (this->Expression < r.Expression) {
    return true;
  }
  if (this->Expression > r.Expression) {
    return false;
  }
  return false;
}

void cmGlobVerificationManager::CacheEntryKey::PrintGlobCommand(
  std::ostream& out, const std::string& cmdVar)
{
  out << "file(GLOB" << (this->Recurse ? "_RECURSE " : " ");
  out << cmdVar << " ";
  if (this->Recurse && this->FollowSymlinks) {
    out << "FOLLOW_SYMLINKS ";
  }
  out << "LIST_DIRECTORIES " << this->ListDirectories << " ";
  if (!this->Relative.empty()) {
    out << "RELATIVE \"" << this->Relative << "\" ";
  }
  out << "\"" << this->Expression << "\")";
}

void cmGlobVerificationManager::AddCacheEntry(
  const bool recurse, const bool listDirectories, const bool followSymlinks,
  const std::string& relative, const std::string& expression,
  const std::vector<std::string>& files, const std::string& variable,
  const cmListFileBacktrace& backtrace)
{
  CacheEntryKey key = CacheEntryKey(recurse, listDirectories, followSymlinks,
                                    relative, expression);
  CacheEntryValue& value = this->Cache[key];
  if (!value.Initialized) {
    value.Files = files;
    value.Initialized = true;
    value.Backtraces.emplace_back(variable, backtrace);
  } else if (value.Initialized && value.Files != files) {
    std::ostringstream message;
    message << std::boolalpha;
    message << "The glob expression\n";
    key.PrintGlobCommand(message, variable);
    backtrace.PrintTitle(message);
    message << "\nwas already present in the glob cache but the directory\n"
               "contents have changed during the configuration run.\n";
    message << "Matching glob expressions:";
    for (auto const& bt : value.Backtraces) {
      message << "\n  " << std::get<0>(bt);
      std::get<1>(bt).PrintTitle(message);
    }
    cmSystemTools::Error(message.str().c_str());
  } else {
    value.Backtraces.emplace_back(variable, backtrace);
  }
}
