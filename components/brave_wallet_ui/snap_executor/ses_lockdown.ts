// Copyright (c) 2024 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

// SES (Secure EcmaScript) lockdown must run before any snap code is executed.
// This hardens the JS environment inside the snap executor iframe, preventing
// prototype pollution and other sandbox escape attacks.

console.error('XXXZZZ ses_lockdown.ts: starting lockdown')
import 'ses'

// Capture the real Date constructor before lockdown tames it.
// dateTaming:'unsafe' doesn't fully restore Date.now() inside Compartments,
// so we pass the original Date as an explicit endowment.
;(globalThis as unknown as Record<string, unknown>)['__snapDate'] = Date

// Capture all Math methods before lockdown tames Math.random().
// @cosmjs uses Math.random() for generating RPC request IDs.
const snapMathRecord: Record<string, unknown> = {}
for (const key of Object.getOwnPropertyNames(Math)) {
  const val = (Math as unknown as Record<string, unknown>)[key]
  snapMathRecord[key] = typeof val === 'function' ? (val as (...a: unknown[]) => unknown).bind(Math) : val
}
;(globalThis as unknown as Record<string, unknown>)['__snapMath'] = snapMathRecord

lockdown({
  errorTaming: 'unsafe',
  overrideTaming: 'severe',
  consoleTaming: 'unsafe',
  // Allow Date.now() inside snap Compartments — snaps need wall-clock time
  // for timeout logic. MetaMask Snaps infrastructure uses the same setting.
  dateTaming: 'unsafe',
})
