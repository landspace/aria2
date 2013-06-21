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
#include "AuthConfigFactory.h"

#include <algorithm>

#include "Option.h"
#include "AuthConfig.h"
#include "Netrc.h"
#include "DefaultAuthResolver.h"
#include "NetrcAuthResolver.h"
#include "prefs.h"
#include "Request.h"
#include "util.h"

namespace aria2 {

namespace {
const std::string AUTH_DEFAULT_USER("anonymous");
const std::string AUTH_DEFAULT_PASSWD("ARIA2USER@");
} // namespace

AuthConfigFactory::AuthConfigFactory() {}

AuthConfigFactory::~AuthConfigFactory() {}

std::shared_ptr<AuthConfig>
AuthConfigFactory::createAuthConfig
(const std::shared_ptr<Request>& request, const Option* op)
{
  if(request->getProtocol() == "http" || request->getProtocol() == "https") {

    if(op->getAsBool(PREF_HTTP_AUTH_CHALLENGE)) {
      if(!request->getUsername().empty()) {
        std::shared_ptr<BasicCred> bc(new BasicCred(request->getUsername(),
                                                 request->getPassword(),
                                                 request->getHost(),
                                                 request->getPort(),
                                                 request->getDir(), true));
        updateBasicCred(bc);
        return createAuthConfig(request->getUsername(), request->getPassword());
      }
      BasicCredSet::iterator i =
        findBasicCred(request->getHost(), request->getPort(),
                      request->getDir());
      if(i == basicCreds_.end()) {
        return std::shared_ptr<AuthConfig>();
      } else {
        return createAuthConfig((*i)->user_, (*i)->password_);
      }
    } else {
      if(!request->getUsername().empty()) {
        return createAuthConfig(request->getUsername(), request->getPassword());
      } else {
        return
          createHttpAuthResolver(op)->resolveAuthConfig(request->getHost());
      }
    }
  } else if(request->getProtocol() == "ftp") {
    if(!request->getUsername().empty()) {
      if(request->hasPassword()) {
        return createAuthConfig(request->getUsername(), request->getPassword());
      } else {
        if(!op->getAsBool(PREF_NO_NETRC)) {
          // First, check we have password corresponding to host and
          // username
          NetrcAuthResolver authResolver;
          authResolver.setNetrc(netrc_);

          std::shared_ptr<AuthConfig> ac =
            authResolver.resolveAuthConfig(request->getHost());
          if(ac && ac->getUser() == request->getUsername()) {
            return ac;
          }
        }
        // We don't have password for host and username. Return
        // password specified by --ftp-passwd
        return
          createAuthConfig(request->getUsername(), op->get(PREF_FTP_PASSWD));
      }
    } else {
      return
        createFtpAuthResolver(op)->resolveAuthConfig(request->getHost());
    }
  } else {
    return std::shared_ptr<AuthConfig>();
  }
}

std::shared_ptr<AuthConfig>
AuthConfigFactory::createAuthConfig(const std::string& user, const std::string& password) const
{
  std::shared_ptr<AuthConfig> ac;
  if(!user.empty()) {
    ac.reset(new AuthConfig(user, password));
  }
  return ac;
}

std::shared_ptr<AuthResolver> AuthConfigFactory::createHttpAuthResolver
(const Option* op) const
{
  AbstractAuthResolver* resolver;
  if(op->getAsBool(PREF_NO_NETRC)) {
    resolver = new DefaultAuthResolver();
  } else {
    NetrcAuthResolver* authResolver(new NetrcAuthResolver());
    authResolver->setNetrc(netrc_);
    authResolver->ignoreDefault();
    resolver = authResolver;
  }
  resolver->setUserDefinedAuthConfig
    (createAuthConfig(op->get(PREF_HTTP_USER), op->get(PREF_HTTP_PASSWD)));
  return std::shared_ptr<AuthResolver>(resolver);
}

std::shared_ptr<AuthResolver> AuthConfigFactory::createFtpAuthResolver
(const Option* op) const
{
  AbstractAuthResolver* resolver;
  if(op->getAsBool(PREF_NO_NETRC)) {
    resolver = new DefaultAuthResolver();
  } else {
    NetrcAuthResolver* authResolver(new NetrcAuthResolver());
    authResolver->setNetrc(netrc_);
    resolver = authResolver;
  }
  resolver->setUserDefinedAuthConfig
    (createAuthConfig(op->get(PREF_FTP_USER), op->get(PREF_FTP_PASSWD)));
  std::shared_ptr<AuthConfig> defaultAuthConfig
    (new AuthConfig(AUTH_DEFAULT_USER, AUTH_DEFAULT_PASSWD));
  resolver->setDefaultAuthConfig(defaultAuthConfig);
  return std::shared_ptr<AuthResolver>(resolver);
}

void AuthConfigFactory::setNetrc(const std::shared_ptr<Netrc>& netrc)
{
  netrc_ = netrc;
}

void AuthConfigFactory::updateBasicCred
(const std::shared_ptr<BasicCred>& basicCred)
{
  BasicCredSet::iterator i = basicCreds_.lower_bound(basicCred);
  if(i != basicCreds_.end() && *(*i) == *basicCred) {
    *(*i) = *basicCred;
  } else {
    basicCreds_.insert(i, basicCred);
  }
}

bool AuthConfigFactory::activateBasicCred
(const std::string& host,
 uint16_t port,
 const std::string& path,
 const Option* op)
{
  BasicCredSet::iterator i = findBasicCred(host, port, path);
  if(i == basicCreds_.end()) {
    std::shared_ptr<AuthConfig> authConfig =
      createHttpAuthResolver(op)->resolveAuthConfig(host);
    if(!authConfig) {
      return false;
    } else {
      std::shared_ptr<BasicCred> bc
        (new BasicCred(authConfig->getUser(), authConfig->getPassword(),
                       host, port, path, true));
      basicCreds_.insert(bc);
      return true;
    }
  } else {
    (*i)->activate();
    return true;
  }
}

AuthConfigFactory::BasicCred::BasicCred
(const std::string& user, const std::string& password,
 const std::string& host, uint16_t port, const std::string& path,
 bool activated):
  user_(user), password_(password),
  host_(host), port_(port), path_(path), activated_(activated)
{
  if(path_.empty() || path_[path_.size()-1] != '/') {
    path_ += "/";
  }
}

void AuthConfigFactory::BasicCred::activate()
{
  activated_ = true;
}

bool AuthConfigFactory::BasicCred::isActivated() const
{
  return activated_;
}

bool AuthConfigFactory::BasicCred::operator==(const BasicCred& cred) const
{
  return host_ == cred.host_ && port_ == cred.port_ && path_ == cred.path_;
}

bool AuthConfigFactory::BasicCred::operator<(const BasicCred& cred) const
{
  return host_ < cred.host_ ||
    (!(cred.host_ < host_) && (port_ < cred.port_ ||
                               (!(cred.port_ < port_) && path_ > cred.path_)));
}

AuthConfigFactory::BasicCredSet::iterator
AuthConfigFactory::findBasicCred
(const std::string& host,
 uint16_t port,
 const std::string& path)
{
  std::shared_ptr<BasicCred> bc(new BasicCred("", "", host, port, path));
  BasicCredSet::iterator i = basicCreds_.lower_bound(bc);
  for(; i != basicCreds_.end() && (*i)->host_ == host && (*i)->port_ == port;
      ++i) {
    if(util::startsWith(bc->path_, (*i)->path_)) {
      return i;
    }
  }
  return basicCreds_.end();
}

} // namespace aria2
