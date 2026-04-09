// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import BraveCore
import BraveStrings
import BraveUI
import SwiftUI
import UIKit

struct ManagePasswordDetailView: View {
  @Environment(\.openURL) private var openURL
  @Environment(\.dismiss) private var dismiss
  @Environment(\.editMode) private var editMode
  @Environment(\.redactionReasons) private var redactionReasons

  @State private var isPasswordRevealed = false
  @State var passwordDraft: ManagePasswordDraft = .init()

  let viewModel: ManagePasswordsViewModel
  let password: CWVPassword
  var navigationTitle: String { URL(string: password.site)?.baseDomain ?? password.title }
  let redactedTitle = Strings.Autofill.managePasswordsTitle

  private var isEditMode: Bool {
    editMode?.wrappedValue == .active
  }

  var body: some View {
    Group {
      if isEditMode {
        ManagePasswordDetailAddEditView(
          isPasswordRevealed: $isPasswordRevealed,
          site: $passwordDraft.site,
          username: $passwordDraft.username,
          passwordValue: $passwordDraft.password
        )
        .onAppear {
          passwordDraft.prepare(with: password)
        }

      } else {
        ManagePasswordDetailReadOnlyView(
          isPasswordRevealed: $isPasswordRevealed,
          password: password
        )
      }
    }
    .scrollContentBackground(.hidden)
    .background((Color(.braveGroupedBackground)))
    .foregroundStyle(Color(braveSystemName: .textPrimary))
    .accessibility(hidden: redactionReasons.contains(.privacy) ? true : false)
    .navigationTitle(redactionReasons.contains(.privacy) ? redactedTitle : navigationTitle)
    .navigationBarTitleDisplayMode(.inline)
    .navigationBarBackButtonHidden(isEditMode)
    .toolbar {
      ToolbarItem(placement: .topBarTrailing) {
        EditButton()
      }

      if isEditMode {
        ToolbarItem(placement: .topBarLeading) {
          Button {
            editMode?.wrappedValue = .inactive
          } label: {
            Label(Strings.done, braveSystemImage: "leo.close")
          }
        }
      }
    }
    .overlay {
      if redactionReasons.contains(.privacy) { Color(.braveGroupedBackground).ignoresSafeArea() }
    }
  }
}

struct ManagePasswordDetailReadOnlyView: View {
  @Environment(\.openURL) private var openURL
  @Binding var isPasswordRevealed: Bool
  let password: CWVPassword

  var body: some View {
    Form {
      Section {
        LabeledContent {
          Menu {
            Button {
              UIPasteboard.general.string = password.site
            } label: {
              Text(Strings.menuItemCopyTitle)
            }
            Button {
              if let url = URL(string: password.site), url.isWebPage() {
                openURL(url)
              }
            } label: {
              Text(Strings.openWebsite)
            }
          } label: {
            Text(password.site).lineLimit(1)
              .contentShape(.rect)
              .foregroundStyle(Color(braveSystemName: .textSecondary))
          }
        } label: {
          Text(Strings.Login.loginInfoDetailsWebsiteFieldTitle)
            .foregroundStyle(Color(braveSystemName: .textPrimary))
        }
        .listRowBackground(Color(.secondaryBraveGroupedBackground))

        LabeledContent {
          Menu {
            Button {
              UIPasteboard.general.setSecureString(password.username ?? "")
            } label: {
              Text(Strings.menuItemCopyTitle)
            }
          } label: {
            Text(password.username ?? "")
              .lineLimit(1)
              .contentShape(.rect)
              .foregroundStyle(Color(braveSystemName: .textSecondary))
          }
        } label: {
          Text(Strings.Login.loginInfoDetailsUsernameFieldTitle)
            .foregroundStyle(Color(braveSystemName: .textPrimary))
        }
        .listRowBackground(Color(.secondaryBraveGroupedBackground))

        LabeledContent {
          HStack(spacing: 8) {
            Menu {
              Button {
                UIPasteboard.general.setSecureString(password.password ?? "")
              } label: {
                Text(Strings.menuItemCopyTitle)
              }
            } label: {
              HStack {
                Spacer()
                if isPasswordRevealed {
                  Text(password.password ?? "")
                    .lineLimit(1)
                } else {
                  Text(String(repeating: "•", count: 8))
                    .lineLimit(1)
                    .allowsHitTesting(false)
                    .accessibility(hidden: true)
                    .multilineTextAlignment(.trailing)
                    .frame(maxWidth: .infinity, alignment: .trailing)
                }
              }
              .contentShape(.rect)
              .foregroundStyle(Color(braveSystemName: .textSecondary))
            }

            Button {
              isPasswordRevealed.toggle()
            } label: {
              Label(
                Strings.Autofill.managePasswordDetailRevealPassword,
                braveSystemImage: isPasswordRevealed ? "leo.eye.on" : "leo.eye.off"
              )
              .foregroundStyle(Color(braveSystemName: .iconInteractive))
              .labelStyle(.iconOnly)
            }
            .buttonStyle(.plain)
          }
        } label: {
          Text(Strings.Login.loginInfoDetailsPasswordFieldTitle)
            .foregroundStyle(Color(braveSystemName: .textPrimary))
        }
        .listRowBackground(Color(.secondaryBraveGroupedBackground))
      }
    }
  }
}

struct ManagePasswordDetailAddEditView: View {
  @FocusState private var isFocused: Bool
  @Binding var isPasswordRevealed: Bool
  @Binding private(set) var site: String
  @Binding private(set) var username: String
  @Binding private(set) var passwordValue: String

  var body: some View {
    Form {
      Section {
        LabeledContent {
          TextField("", text: $site)
            .textContentType(.URL)
            .textInputAutocapitalization(.never)
            .autocorrectionDisabled()
            .accessibilityLabel(Strings.Login.loginInfoDetailsWebsiteFieldTitle)
            .multilineTextAlignment(.trailing)
            .focused($isFocused)
            .foregroundStyle(Color(braveSystemName: .textSecondary))
        } label: {
          Text(Strings.Login.loginInfoDetailsWebsiteFieldTitle)
            .foregroundStyle(Color(braveSystemName: .textPrimary))
        }
        .listRowBackground(Color(.secondaryBraveGroupedBackground))

        LabeledContent {
          TextField("", text: $username)
            .textContentType(.username)
            .textInputAutocapitalization(.never)
            .autocorrectionDisabled()
            .accessibilityLabel(Strings.Login.loginInfoDetailsUsernameFieldTitle)
            .multilineTextAlignment(.trailing)
            .foregroundStyle(Color(braveSystemName: .textSecondary))
        } label: {
          Text(Strings.Login.loginInfoDetailsUsernameFieldTitle)
            .foregroundStyle(Color(braveSystemName: .textPrimary))
        }
        .listRowBackground(Color(.secondaryBraveGroupedBackground))

        LabeledContent {
          HStack(spacing: 8) {
            Group {
              if isPasswordRevealed {
                TextField(
                  Strings.Autofill.managePasswordDetailInputPasswordPlaceholder,
                  text: $passwordValue
                )
                .textContentType(.password)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .accessibilityLabel(Strings.Login.loginInfoDetailsPasswordFieldTitle)
              } else {
                SecureField(
                  Strings.Autofill.managePasswordDetailInputPasswordPlaceholder,
                  text: $passwordValue
                )
                .textContentType(.password)
                .accessibilityLabel(Strings.Login.loginInfoDetailsPasswordFieldTitle)
              }
            }
            .multilineTextAlignment(.trailing)
            .foregroundStyle(Color(braveSystemName: .textSecondary))

            Button {
              isPasswordRevealed.toggle()
            } label: {
              Label(
                Strings.Autofill.managePasswordDetailRevealPassword,
                braveSystemImage: isPasswordRevealed ? "leo.eye.on" : "leo.eye.off"
              )
              .foregroundStyle(Color(braveSystemName: .iconInteractive))
              .labelStyle(.iconOnly)
            }
            .buttonStyle(.plain)
          }
        } label: {
          Text(Strings.Login.loginInfoDetailsPasswordFieldTitle)
            .foregroundStyle(Color(braveSystemName: .textPrimary))
        }
        .listRowBackground(Color(.secondaryBraveGroupedBackground))
      }
    }
    .task {
      try? await Task.sleep(for: .milliseconds(500))
      isFocused = true
    }
  }
}
