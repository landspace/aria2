/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2006 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#ifndef D_REQUEST_GROUP_H
#define D_REQUEST_GROUP_H

#include "common.h"

#include <string>
#include <algorithm>
#include <vector>
#include <memory>

#include "TransferStat.h"
#include "TimeA2.h"
#include "Request.h"
#include "error_code.h"
#include "MetadataInfo.h"
#include "GroupId.h"

namespace aria2 {

class DownloadEngine;
class SegmentMan;
class Command;
class DownloadCommand;
class DownloadContext;
class PieceStorage;
class BtProgressInfoFile;
class Dependency;
class PreDownloadHandler;
class PostDownloadHandler;
class DiskWriterFactory;
class Option;
class RequestGroup;
class CheckIntegrityEntry;
struct DownloadResult;
class URISelector;
class URIResult;
class RequestGroupMan;
#ifdef ENABLE_BITTORRENT
class BtRuntime;
class PeerStorage;
#endif // ENABLE_BITTORRENT

class RequestGroup {
public:
  enum HaltReason {
    NONE,
    SHUTDOWN_SIGNAL,
    USER_REQUEST
  };
  enum State {
    // Waiting in the reserved queue
    STATE_WAITING,
    // Download has begun
    STATE_ACTIVE
  };
private:
  std::shared_ptr<GroupId> gid_;

  int state_;

  std::shared_ptr<Option> option_;

  int numConcurrentCommand_;

  /**
   * This is the number of connections used in streaming protocol(http/ftp)
   */
  int numStreamConnection_;

  int numStreamCommand_;

  int numCommand_;

  std::shared_ptr<SegmentMan> segmentMan_;

  std::shared_ptr<DownloadContext> downloadContext_;

  std::shared_ptr<PieceStorage> pieceStorage_;

  bool saveControlFile_;

  std::shared_ptr<BtProgressInfoFile> progressInfoFile_;

  std::shared_ptr<DiskWriterFactory> diskWriterFactory_;

  std::shared_ptr<Dependency> dependency_;

  bool fileAllocationEnabled_;

  bool preLocalFileCheckEnabled_;

  bool haltRequested_;

  bool forceHaltRequested_;

  HaltReason haltReason_;

  bool pauseRequested_;

  std::vector<std::shared_ptr<PreDownloadHandler> > preDownloadHandlers_;

  std::vector<std::shared_ptr<PostDownloadHandler> > postDownloadHandlers_;

  std::shared_ptr<URISelector> uriSelector_;

  Time lastModifiedTime_;

  int fileNotFoundCount_;

  // Timeout used for HTTP/FTP downloads.
  time_t timeout_;

#ifdef ENABLE_BITTORRENT
  BtRuntime* btRuntime_;

  PeerStorage* peerStorage_;
#endif // ENABLE_BITTORRENT

  // This flag just indicates that the downloaded file is not saved disk but
  // just sits in memory.
  bool inMemoryDownload_;

  int maxDownloadSpeedLimit_;

  int maxUploadSpeedLimit_;

  error_code::Value lastErrorCode_;

  // If this download generates another downloads when completed(for
  // example, downloads generated by PostDownloadHandler), this field
  // has the GID of generated RequestGroups. empty list means there is
  // no such RequestGroup.
  std::vector<a2_gid_t> followedByGIDs_;

  // If this download is a part of another download(for example,
  // downloading torrent file described in Metalink file), this field
  // has the GID of parent RequestGroup. 0 means this is a parent
  // RequestGroup.
  a2_gid_t belongsToGID_;

  std::shared_ptr<MetadataInfo> metadataInfo_;

  RequestGroupMan* requestGroupMan_;

  int resumeFailureCount_;

  void validateFilename(const std::string& expectedFilename,
                        const std::string& actualFilename) const;

  void initializePreDownloadHandler();

  void initializePostDownloadHandler();

  bool tryAutoFileRenaming();

  // Returns the result code of this RequestGroup.  If the download
  // finished, then returns error_code::FINISHED.  If the
  // download didn't finish and error result is available in
  // _uriResults, then last result code is returned.  Otherwise
  // returns error_code::UNKNOWN_ERROR.
  error_code::Value downloadResult() const;

  void removeDefunctControlFile
  (const std::shared_ptr<BtProgressInfoFile>& progressInfoFile);

public:
  RequestGroup(const std::shared_ptr<GroupId>& gid,
               const std::shared_ptr<Option>& option);

  ~RequestGroup();

  bool isCheckIntegrityReady();

  const std::shared_ptr<SegmentMan>& getSegmentMan() const
  {
    return segmentMan_;
  }

  std::shared_ptr<CheckIntegrityEntry> createCheckIntegrityEntry();

  // Returns first bootstrap commands to initiate a download.
  // If this is HTTP/FTP download and file size is unknown, only 1 command
  // (usually, HttpInitiateConnection or FtpInitiateConnection) will be created.
  void createInitialCommand(std::vector<Command*>& commands,
                            DownloadEngine* e);

  void createNextCommandWithAdj(std::vector<Command*>& commands,
                                DownloadEngine* e, int numAdj);

  void createNextCommand(std::vector<Command*>& commands,
                         DownloadEngine* e, int numCommand);

  void createNextCommand(std::vector<Command*>& commands, DownloadEngine* e);

  bool downloadFinished() const;

  bool allDownloadFinished() const;

  void closeFile();

  std::string getFirstFilePath() const;

  int64_t getTotalLength() const;

  int64_t getCompletedLength() const;

  /**
   * Compares expected filename with specified actualFilename.
   * The expected filename refers to FileEntry::getBasename() of the first
   * element of DownloadContext::getFileEntries()
   */
  void validateFilename(const std::string& actualFilename) const;

  void validateTotalLength(int64_t expectedTotalLength,
                           int64_t actualTotalLength) const;

  void validateTotalLength(int64_t actualTotalLength) const;

  void setNumConcurrentCommand(int num)
  {
    numConcurrentCommand_ = num;
  }

  int getNumConcurrentCommand() const
  {
    return numConcurrentCommand_;
  }

  a2_gid_t getGID() const
  {
    return gid_->getNumericId();
  }

  const std::shared_ptr<GroupId>& getGroupId() const
  {
    return gid_;
  }

  TransferStat calculateStat() const;

  const std::shared_ptr<DownloadContext>& getDownloadContext() const
  {
    return downloadContext_;
  }

  // This function also calls
  // downloadContext->setOwnerRequestGroup(this).
  void setDownloadContext(const std::shared_ptr<DownloadContext>& downloadContext);

  const std::shared_ptr<PieceStorage>& getPieceStorage() const
  {
    return pieceStorage_;
  }

  void setPieceStorage(const std::shared_ptr<PieceStorage>& pieceStorage);

  void setProgressInfoFile(const std::shared_ptr<BtProgressInfoFile>& progressInfoFile);

  void increaseStreamCommand();

  void decreaseStreamCommand();

  void increaseStreamConnection();

  void decreaseStreamConnection();

  int getNumConnection() const;

  void increaseNumCommand();

  void decreaseNumCommand();

  int getNumCommand() const
  {
    return numCommand_;
  }

  // TODO is it better to move the following 2 methods to SingleFileDownloadContext?
  void setDiskWriterFactory(const std::shared_ptr<DiskWriterFactory>& diskWriterFactory);

  const std::shared_ptr<DiskWriterFactory>& getDiskWriterFactory() const
  {
    return diskWriterFactory_;
  }

  void setFileAllocationEnabled(bool f)
  {
    fileAllocationEnabled_ = f;
  }

  bool isFileAllocationEnabled() const
  {
    return fileAllocationEnabled_;
  }

  bool needsFileAllocation() const;

  /**
   * Setting preLocalFileCheckEnabled_ to false, then skip the check to see
   * if a file is already exists and control file exists etc.
   * Always open file with DiskAdaptor::initAndOpenFile()
   */
  void setPreLocalFileCheckEnabled(bool f)
  {
    preLocalFileCheckEnabled_ = f;
  }

  bool isPreLocalFileCheckEnabled() const
  {
    return preLocalFileCheckEnabled_;
  }

  void setHaltRequested(bool f, HaltReason = SHUTDOWN_SIGNAL);

  void setForceHaltRequested(bool f, HaltReason = SHUTDOWN_SIGNAL);

  bool isHaltRequested() const
  {
    return haltRequested_;
  }

  bool isForceHaltRequested() const
  {
    return forceHaltRequested_;
  }

  void setPauseRequested(bool f);

  bool isPauseRequested() const
  {
    return pauseRequested_;
  }

  void dependsOn(const std::shared_ptr<Dependency>& dep);

  bool isDependencyResolved();

  void releaseRuntimeResource(DownloadEngine* e);

  void postDownloadProcessing(std::vector<std::shared_ptr<RequestGroup> >& groups);

  void addPostDownloadHandler(const std::shared_ptr<PostDownloadHandler>& handler);

  void clearPostDownloadHandler();

  void preDownloadProcessing();

  void addPreDownloadHandler(const std::shared_ptr<PreDownloadHandler>& handler);

  void clearPreDownloadHandler();

  void processCheckIntegrityEntry(std::vector<Command*>& commands,
                                  const std::shared_ptr<CheckIntegrityEntry>& entry,
                                  DownloadEngine* e);

  // Initializes pieceStorage_ and segmentMan_.  We guarantee that
  // either both of pieceStorage_ and segmentMan_ are initialized or
  // they are not.
  void initPieceStorage();

  void dropPieceStorage();

  bool downloadFinishedByFileLength();

  void loadAndOpenFile(const std::shared_ptr<BtProgressInfoFile>& progressInfoFile);

  void shouldCancelDownloadForSafety();

  void adjustFilename(const std::shared_ptr<BtProgressInfoFile>& infoFile);

  std::shared_ptr<DownloadResult> createDownloadResult() const;

  const std::shared_ptr<Option>& getOption() const
  {
    return option_;
  }

  void reportDownloadFinished();

  void setURISelector(const std::shared_ptr<URISelector>& uriSelector);

  const std::shared_ptr<URISelector>& getURISelector() const
  {
    return uriSelector_;
  }

  void applyLastModifiedTimeToLocalFiles();

  void updateLastModifiedTime(const Time& time);

  void increaseAndValidateFileNotFoundCount();

  // Just set inMemoryDownload flag true.
  void markInMemoryDownload();

  // Returns inMemoryDownload flag.
  bool inMemoryDownload() const
  {
    return inMemoryDownload_;
  }

  void setTimeout(time_t timeout);

  time_t getTimeout() const
  {
    return timeout_;
  }

  // Returns true if current download speed exceeds
  // maxDownloadSpeedLimit_.  Always returns false if
  // maxDownloadSpeedLimit_ == 0.  Otherwise returns false.
  bool doesDownloadSpeedExceed();

  // Returns true if current upload speed exceeds
  // maxUploadSpeedLimit_. Always returns false if
  // maxUploadSpeedLimit_ == 0. Otherwise returns false.
  bool doesUploadSpeedExceed();

  int getMaxDownloadSpeedLimit() const
  {
    return maxDownloadSpeedLimit_;
  }

  void setMaxDownloadSpeedLimit(int speed)
  {
    maxDownloadSpeedLimit_ = speed;
  }

  int getMaxUploadSpeedLimit() const
  {
    return maxUploadSpeedLimit_;
  }

  void setMaxUploadSpeedLimit(int speed)
  {
    maxUploadSpeedLimit_ = speed;
  }

  void setLastErrorCode(error_code::Value code)
  {
    lastErrorCode_ = code;
  }

  error_code::Value getLastErrorCode() const
  {
    return lastErrorCode_;
  }

  void saveControlFile() const;

  void removeControlFile() const;

  void enableSaveControlFile() { saveControlFile_ = true; }

  void disableSaveControlFile() { saveControlFile_ = false; }

  template<typename InputIterator>
  void followedBy(InputIterator groupFirst, InputIterator groupLast)
  {
    followedByGIDs_.clear();
    for(; groupFirst != groupLast; ++groupFirst) {
      followedByGIDs_.push_back((*groupFirst)->getGID());
    }
  }

  const std::vector<a2_gid_t>& followedBy() const
  {
    return followedByGIDs_;
  }

  void belongsTo(a2_gid_t gid)
  {
    belongsToGID_ = gid;
  }

  a2_gid_t belongsTo() const
  {
    return belongsToGID_;
  }

  void setRequestGroupMan(RequestGroupMan* requestGroupMan)
  {
    requestGroupMan_ = requestGroupMan;
  }

  RequestGroupMan* getRequestGroupMan()
  {
    return requestGroupMan_;
  }

  int getResumeFailureCount() const
  {
    return resumeFailureCount_;
  }

  void increaseResumeFailureCount()
  {
    ++resumeFailureCount_;
  }

  bool p2pInvolved() const;

  void setMetadataInfo(const std::shared_ptr<MetadataInfo>& info)
  {
    metadataInfo_ = info;
  }

  const std::shared_ptr<MetadataInfo>& getMetadataInfo() const
  {
    return metadataInfo_;
  }

  int getState() const
  {
    return state_;
  }

  void setState(int state)
  {
    state_ = state;
  }
};

} // namespace aria2

#endif // D_REQUEST_GROUP_H
