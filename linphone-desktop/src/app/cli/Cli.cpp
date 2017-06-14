/*
 * Cli.cpp
 * Copyright (C) 2017  Belledonne Communications, Grenoble, France
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Created on: June 6, 2017
 *      Author: Nicolas Follet
 */

#include <stdexcept>
#include <linphone++/linphone.hh>

#include "../../components/core/CoreManager.hpp"
#include "../../utils/Utils.hpp"
#include "../App.hpp"

#include "Cli.hpp"

using namespace std;


// =============================================================================
// API.
// =============================================================================

static void cliShow (const QHash<QString, QString> &) {
  App *app = App::getInstance();
  app->smartShowWindow(app->getMainWindow());
}

static void cliCall (const QHash<QString, QString> &args) {
  CoreManager::getInstance()->getCallsListModel()->launchAudioCall(args["sip-address"]);
}

static void cliJoinConference (const QHash<QString, QString> &args) {
  CoreManager::getInstance()->getCallsListModel()->launchAudioCall(args["sip-address"]);
}

static void cliInitiateConference (const QHash<QString, QString> &args) {
  std::shared_ptr<linphone::Conference> conf = CoreManager::getInstance()->getCore()->createConferenceWithParams(
        CoreManager::getInstance()->getCore()->createConferenceParams()
        );
  std::pair<std::string,std::shared_ptr<linphone::Conference>> aa;
  aa.first = Utils::appStringToCoreString(args["conferenceID"]);
  aa.second = conf ;

  //Create a Conference with conferenceID header (in args["conferenceID"]) and set the vew to the "waiting call vew"
}

// =============================================================================

Cli::Command::Command (const QString &functionName, const QString &description, Cli::Function function, const QHash<QString, Cli::Argument> &argsScheme) :
  mFunctionName(functionName),
  mDescription(description),
  mFunction(function),
  mArgsScheme(argsScheme) {}

void Cli::Command::execute (const QHash<QString, QString> &args) {
  for (const auto &argName : mArgsScheme.keys()) {
    if (!args.contains(argName) && !mArgsScheme[argName].isOptional) {
      qWarning() << QStringLiteral("Missing unoptional argument for command: `%1 (%2)`.")
        .arg(mFunctionName).arg(argName);
      return;
    }
  }

  (*mFunction)(args);
}

void Cli::Command::executeUri(shared_ptr<linphone::Address> address){

  QHash<QString, QString> args;
  args["sip-address"] = Utils::coreStringToAppString(address->asString());
  for (const auto &argName : mArgsScheme.keys()) {
    if(Utils::appStringToCoreString(argName)!="sipAddress"){
      if(address->getHeader(Utils::appStringToCoreString(argName)).empty() &&
         !mArgsScheme[argName].isOptional) {
        qWarning() << QStringLiteral("Missing unoptional argument for method: `%1 (%2)`.")
            .arg(mFunctionName).arg(argName);
        return;
      }
      args[argName] = Utils::coreStringToAppString(address->getHeader(Utils::appStringToCoreString(argName)));
    }
  }
  (*mFunction)(args);
}


// =============================================================================

// FIXME: Do not accept args without value like: cmd toto.
// In the future `toto` could be a boolean argument.
QRegExp Cli::mRegExpArgs("(?:(?:(\\w+)\\s*)=\\s*(?:\"([^\"\\\\]*(?:\\\\.[^\"\\\\]*)*)\"|([^\\s]+)\\s*))");
QRegExp Cli::mRegExpFunctionName("^\\s*(\\w+)\\s*");

Cli::Cli (QObject *parent) : QObject(parent) {
  addCommand("show", tr("showFunctionDescription"), ::cliShow);
  addCommand("call", tr("showFunctionCall"), ::cliCall, {
    { "sip-address" }
  });
  addCommand("join-conference", tr("joinConferenceFunctionDescription"), ::cliJoinConference, {
    { "sip-address" }, { "conference-id" }
  });
  addCommand("initiateConference", tr("initiateConferenceFunctionDescription"), ::cliInitiateConference, {
    { "sip-address" }, { "conference-id" }
  });
}


// -----------------------------------------------------------------------------

void Cli::addCommand (const QString &functionName, const QString &description, Function function, const QHash<QString, Argument> &argsScheme) noexcept {
  if (mCommands.contains(functionName))
    qWarning() << QStringLiteral("Command already exists: `%1`.").arg(functionName);
  else
    mCommands[functionName] = Cli::Command(functionName, description, function, argsScheme);
}

// -----------------------------------------------------------------------------

void Cli::executeCommand (const QString &command) noexcept {

  //tests if the command is a sip uri.
  shared_ptr<linphone::Address> address = linphone::Factory::get()->createAddress(Utils::appStringToCoreString(command));
  if (address && (address->getScheme()=="sip" || address->getScheme()=="sip-linphone")) {

    const QString methodName = Utils::coreStringToAppString(address->getHeader("method"));
    if (methodName.isEmpty() || !mCommands.contains(methodName)){
      qWarning() << QStringLiteral("method unknown");
      return;
    }

    //calls the appropriate function of the method
    mCommands[methodName].executeUri(address);
    return;
  }

  if (address && !address->getScheme().empty()){
    qWarning() << QStringLiteral("bad uri protocol, different from sip or sip-linphone: `%1`").arg(Utils::coreStringToAppString(address->getScheme()));
    return;
  }

  const QString &functionName = parseFunctionName(command);
  if (functionName.isEmpty())
    return;

  bool soFarSoGood;
  const QHash<QString, QString> &args = parseArgs(command, functionName, soFarSoGood);
  if (soFarSoGood)
    mCommands[functionName].execute(args);
}

// -----------------------------------------------------------------------------

const QString Cli::parseFunctionName (const QString &command) noexcept {

  mRegExpFunctionName.indexIn(command);

  const QStringList &texts = mRegExpFunctionName.capturedTexts();
  if (texts.size() < 2) {
    qWarning() << QStringLiteral("Unable to parse function name of command: `%1`.").arg(command);
    return QString("");
  }

  const QString functionName = texts[1];
  if (!mCommands.contains(functionName)) {
    qWarning() << QStringLiteral("This command doesn't exist: `%1`.").arg(functionName);
    return QString("");
  }

  return functionName;
}


const QHash<QString, QString> Cli::parseArgs (const QString &command, const QString functionName, bool &soFarSoGood) noexcept {

  QHash<QString, QString> args;
  int pos = 0;

  soFarSoGood = true;
  args["method"] = functionName;
  while ((pos = mRegExpArgs.indexIn(command, pos)) != -1) {
    pos += mRegExpArgs.matchedLength();
    if (!mCommands[functionName].argNameExists(mRegExpArgs.cap(1))) {
      qWarning() << QStringLiteral("Command with invalid argument(s): `%1 (%2)`.")
        .arg(functionName).arg(mRegExpArgs.cap(1));

      soFarSoGood = false;
      return args;
    }

    args[mRegExpArgs.cap(1)] = (mRegExpArgs.cap(2).isEmpty() ? mRegExpArgs.cap(3) : mRegExpArgs.cap(2));
  }

  return args;
}
