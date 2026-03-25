// Copyright (c) 2024 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

// snap_executor.ts runs inside a chrome-untrusted://snap-executor/ iframe.
// One instance is created per snap. SES lockdown() has already been called
// via ses_lockdown.bundle.js before this script executes. Compartment and
// harden are declared by the ses package types and available as globals.

console.error('XXXZZZ snap_executor.ts: bundle loaded')

const INVOKE_TIMEOUT_MS = 60_000

// Each iframe is dedicated to exactly one snap. Read the snap ID from the URL
// so we know our identity before any postMessage arrives.
// e.g. chrome-untrusted://snap-executor/?snap_id=npm%3A%40chainsafe%2Ffilsnap
const _iframeSnapId: string | null = new URLSearchParams(location.search).get('snap_id')
console.error('XXXZZZ snap_executor.ts: iframe snap_id=', _iframeSnapId)

// In-memory state store for snap_manageState — keyed by snapId.
// Mirrors MetaMask's execution environment which handles state locally.
const snapStateStore = new Map<string, unknown>()

// In-memory interface store for snap_createInterface / snap_updateInterface.
// Maps interface ID → component tree. Keyed locally; MetaMask does the same.
const snapInterfaces = new Map<string, unknown>()

// Map from snap ID to the bundle URL path served from this origin.
const SNAP_BUNDLE_URLS: Record<string, string> = {
  'npm:@cosmsnap/snap': 'snap-bundles/cosmos.js',
  'npm:filsnap': 'snap-bundles/filecoin.js',
  'npm:@polkagate/snap': 'snap-bundles/polkadot.js',
}

// The snap's onRpcRequest handler, set after load_snap succeeds.
let snapOnRpcRequest:
  | ((args: {
      origin: string
      request: { method: string; params: unknown }
    }) => Promise<unknown>)
  | null = null

// The snap's onHomePage handler (optional — only snaps with endowment:page-home).
let snapOnHomePage: (() => Promise<unknown>) | null = null

// The snap's onUserInput handler (optional — called when user interacts with
// buttons, inputs, forms, or dropdowns in the snap's UI).
let snapOnUserInput:
  | ((args: { id: string; event: unknown; context?: unknown }) => Promise<void>)
  | null = null

// In-memory form state for snap_getInterfaceState — tracks current input values
// per interface. Keyed by interfaceId → { fieldName: value }.
const snapInterfaceStates = new Map<string, Record<string, string>>()

// Pending snap.request() calls waiting for a response from the parent.
const pendingSnapRequests = new Map<
  string,
  { resolve: (value: unknown) => void; reject: (reason: Error) => void }
>()

function generateRequestId(): string {
  return `${Date.now()}-${Math.random().toString(36).slice(2)}`
}

function sendToParent(message: Record<string, unknown>): void {
  window.parent.postMessage(message, '*')
}

function sendResponse(
  requestId: string,
  result?: unknown,
  error?: string,
): void {
  sendToParent({ type: 'response', requestId, result, error })
}

// Returns true if s is a valid, decodable standard-base64 string.
// base64-js (used by @cosmjs/encoding) requires length to be a multiple of 4,
// so we check that here in addition to the character-set test.
function isStandardBase64(s: string): boolean {
  return s.length % 4 === 0 && /^[a-zA-Z0-9+/]*={0,2}$/.test(s)
}

// Walk an arbitrary JSON value and base64-encode any EventAttribute key/value
// that is a plain string (CometBFT v0.38+ changed bytes→string in the proto).
function normaliseEventAttrs(v: unknown): void {
  if (!v || typeof v !== 'object') return
  if (Array.isArray(v)) {
    for (const item of v) normaliseEventAttrs(item)
    return
  }
  const obj = v as Record<string, unknown>
  if (typeof obj['key'] === 'string' && typeof obj['value'] === 'string') {
    if (!isStandardBase64(obj['key']))   obj['key']   = btoa(obj['key'])
    if (!isStandardBase64(obj['value'])) obj['value'] = btoa(obj['value'])
  }
  for (const val of Object.values(obj)) normaliseEventAttrs(val)
}

// Fetch proxy that patches CometBFT v0.38 JSON-RPC responses so the snap's
// bundled @cosmjs/tendermint-rpc v0.30.x can parse event attributes.
async function cometBftFetchProxy(
  input: RequestInfo | URL,
  init?: RequestInit,
): Promise<Response> {
  const response = await fetch(input, init)
  const contentType = response.headers.get('content-type') ?? ''
  if (!contentType.includes('application/json')) return response
  const text = await response.text()
  try {
    const json = JSON.parse(text) as unknown
    normaliseEventAttrs(json)
    return new Response(JSON.stringify(json), {
      status: response.status,
      statusText: response.statusText,
      headers: response.headers,
    })
  } catch {
    return new Response(text, {
      status: response.status,
      statusText: response.statusText,
      headers: response.headers,
    })
  }
}

async function handleLoadSnap(
  snapId: string,
  requestId: string,
): Promise<void> {
  console.error('XXXZZZ snap_executor handleLoadSnap snapId=', snapId)
  try {
    const bundleUrl = SNAP_BUNDLE_URLS[snapId]
    if (!bundleUrl) {
      sendResponse(requestId, undefined, `No bundle registered for snap: ${snapId}`)
      return
    }
    console.error('XXXZZZ snap_executor: fetching bundle from', bundleUrl)
    const resp = await fetch(bundleUrl)
    if (!resp.ok) {
      sendResponse(requestId, undefined, `Failed to fetch snap bundle: ${resp.status} ${resp.statusText}`)
      return
    }
    const source = await resp.text()
    console.error('XXXZZZ snap_executor: fetched source length=', source.length)
    // snap.request() — called by snap code to invoke core APIs (e.g. key derivation).
    // Sends a snap_request message to the wallet page, which relays it to C++ core,
    // then resolves/rejects when the response arrives via snap_request_response.
    const snapRequestFn = (params: {
      method: string
      params?: unknown
    }): Promise<unknown> => {
      // Handle snap_manageState locally — state is stored per-snap in memory.
      // The C++ side never sees these calls (mirrors MetaMask's snap executor).
      if (params.method === 'snap_manageState') {
        const p = params.params as {
          operation: 'get' | 'update' | 'clear'
          newState?: unknown
        }
        if (p.operation === 'get') {
          return Promise.resolve(snapStateStore.get(snapId) ?? null)
        }
        if (p.operation === 'update') {
          snapStateStore.set(snapId, p.newState)
          return Promise.resolve(null)
        }
        if (p.operation === 'clear') {
          snapStateStore.delete(snapId)
          return Promise.resolve(null)
        }
        return Promise.reject(new Error(`snap_manageState: unknown operation ${(p as { operation: string }).operation}`))
      }

      // Handle snap_createInterface locally — store the component tree and
      // return a generated ID. The snap's onHomePage may return { id } instead
      // of { content }, pointing to an interface created this way.
      if (params.method === 'snap_createInterface') {
        const p = params.params as { ui?: unknown } | undefined
        const ui = (p as Record<string, unknown>)?.['ui'] ?? p
        const interfaceId = generateRequestId()
        snapInterfaces.set(interfaceId, ui)
        return Promise.resolve(interfaceId)
      }

      // Handle snap_updateInterface locally — replace stored component tree.
      if (params.method === 'snap_updateInterface') {
        const p = params.params as { id: string; ui?: unknown } | undefined
        const obj = p as Record<string, unknown> | undefined
        const id = obj?.['id'] as string | undefined
        const ui = obj?.['ui']
        if (id) {
          snapInterfaces.set(id, ui)
        }
        return Promise.resolve(null)
      }

      // Handle snap_getInterfaceState locally — return current form field values.
      if (params.method === 'snap_getInterfaceState') {
        const p = params.params as { id: string } | undefined
        const id = (p as Record<string, unknown>)?.['id'] as string | undefined
        return Promise.resolve(id ? (snapInterfaceStates.get(id) ?? {}) : {})
      }

      // Relay all other snap.request() calls to the parent (C++).
      return new Promise((resolve, reject) => {
        const snapReqId = generateRequestId()
        pendingSnapRequests.set(snapReqId, { resolve, reject })
        sendToParent({
          type: 'snap_request',
          snapId,
          method: params.method,
          params: params.params,
          requestId: snapReqId,
        })
      })
    }

    // Expose snap exports so the Compartment can capture them.
    // CommonJS-style snaps set module.exports.onRpcRequest.
    const snapExports: Record<string, unknown> = {}
    const snapModule = { exports: snapExports }

    // Retrieve the original Date constructor captured before SES lockdown.
    // dateTaming:'unsafe' does not fully restore Date.now() inside Compartments.
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const SnapDate: typeof Date = (globalThis as any)['__snapDate'] ?? Date

    // Retrieve Math captured before SES lockdown tamed it.
    const snapMath: Record<string, unknown> =
      (globalThis as unknown as Record<string, unknown>)['__snapMath'] as Record<string, unknown>
      ?? Math

    const compartment = new Compartment({
      module: snapModule,
      exports: snapExports,
      snap: harden({ request: snapRequestFn }),
      console: harden({
        log: console.log.bind(console),
        warn: console.warn.bind(console),
        error: console.error.bind(console),
        info: console.info.bind(console),
        debug: console.debug.bind(console),
      }),
      Date: SnapDate,
      Math: harden(snapMath),
      setTimeout: setTimeout.bind(globalThis),
      clearTimeout: clearTimeout.bind(globalThis),
      setInterval: setInterval.bind(globalThis),
      clearInterval: clearInterval.bind(globalThis),
      Promise: Promise,
      // Encoding / crypto globals used by @cosmjs
      atob: atob.bind(globalThis),
      btoa: btoa.bind(globalThis),
      // Typed array constructors that SES may not expose in Compartment scope
      Uint8Array: Uint8Array,
      Int8Array: Int8Array,
      Uint16Array: Uint16Array,
      Int16Array: Int16Array,
      Uint32Array: Uint32Array,
      Int32Array: Int32Array,
      Float32Array: Float32Array,
      Float64Array: Float64Array,
      ArrayBuffer: ArrayBuffer,
      DataView: DataView,
      // crypto.getRandomValues used by some @cosmjs internals
      crypto: globalThis.crypto,
      TextEncoder: harden(TextEncoder),
      TextDecoder: harden(TextDecoder),
      // Network APIs — needed by snaps with endowment:network-access.
      // The CometBFT proxy is only needed for the Cosmos snap: it normalises
      // v0.38+ plain-string event attributes to base64 for @cosmjs v0.30.x.
      // Other snaps (polkagate, filsnap) use plain fetch so their JSON-RPC
      // responses are not inadvertently mangled.
      fetch: snapId === 'npm:@cosmsnap/snap' ? cometBftFetchProxy : fetch,
      Headers: harden(Headers),
      Request: harden(Request),
      Response: harden(Response),
      AbortController: harden(AbortController),
      AbortSignal: harden(AbortSignal),
      // URL APIs — needed by snaps that parse/construct URLs (e.g. filsnap,
      // polkagate). SES does not expose these in Compartment scope by default.
      URL: URL,
      URLSearchParams: URLSearchParams,
      // WebAssembly — required by snaps with endowment:webassembly (e.g.
      // @polkagate/snap uses @polkadot/wasm-crypto for sr25519/ed25519).
      WebAssembly: WebAssembly,
      // WebSocket — @polkadot/api uses WebSocket for substrate RPC connections.
      WebSocket: WebSocket,
    })

    console.error('XXXZZZ snap_executor: calling compartment.evaluate')
    compartment.evaluate(source)

    // After evaluate, read exports from snapModule.exports — webpack-style
    // bundles do `module.exports = { onRpcRequest, onHomePage, ... }` which
    // REPLACES the module.exports reference rather than mutating snapExports.
    // CommonJS snaps that mutate exports directly also work since snapModule.exports
    // starts as snapExports and may still be the same object.
    const actualExports = (snapModule.exports ?? snapExports) as Record<string, unknown>
    console.error('XXXZZZ snap_executor: evaluate done, exports keys=', Object.keys(actualExports))

    const rpcHandler = actualExports['onRpcRequest']
    if (typeof rpcHandler === 'function') {
      snapOnRpcRequest = rpcHandler as typeof snapOnRpcRequest
    }

    const homeHandler = actualExports['onHomePage']
    if (typeof homeHandler === 'function') {
      snapOnHomePage = homeHandler as typeof snapOnHomePage
    }

    const userInputHandler = actualExports['onUserInput']
    if (typeof userInputHandler === 'function') {
      snapOnUserInput = userInputHandler as typeof snapOnUserInput
    }

    if (!snapOnRpcRequest && !snapOnHomePage) {
      console.error('XXXZZZ snap_executor ERROR: snap exports neither onRpcRequest nor onHomePage')
      sendResponse(requestId, undefined, 'Snap does not export onRpcRequest or onHomePage')
      return
    }

    console.error('XXXZZZ snap_executor: snap loaded OK, onRpcRequest=', !!snapOnRpcRequest, 'onHomePage=', !!snapOnHomePage, 'onUserInput=', !!snapOnUserInput)
    sendResponse(requestId, true)
  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err)
    const stack = err instanceof Error ? err.stack : ''
    console.error('XXXZZZ snap_executor ERROR in handleLoadSnap:', msg, stack)
    sendResponse(requestId, undefined, `Failed to load snap: ${msg}`)
  }
}

async function handleInvoke(
  method: string,
  params: unknown,
  origin: string,
  requestId: string,
): Promise<void> {
  if (typeof snapOnRpcRequest !== 'function') {
    sendResponse(requestId, undefined, 'Snap not loaded')
    return
  }

  let settled = false

  const timeoutId = setTimeout(() => {
    if (!settled) {
      settled = true
      sendResponse(
        requestId,
        undefined,
        `Snap RPC timed out after ${INVOKE_TIMEOUT_MS}ms`,
      )
    }
  }, INVOKE_TIMEOUT_MS)

  try {
    const result = await snapOnRpcRequest({
      origin,
      request: { method, params },
    })
    console.error('XXXZZZ snap_executor handleInvoke result:', JSON.stringify(result)?.slice(0, 300))
    clearTimeout(timeoutId)
    if (!settled) {
      settled = true
      sendResponse(requestId, result)
    }
  } catch (err) {
    clearTimeout(timeoutId)
    if (!settled) {
      settled = true
      const msg = err instanceof Error ? err.message : String(err)
      sendResponse(requestId, undefined, msg)
    }
  }
}

async function handleGetHomePage(requestId: string): Promise<void> {
  if (typeof snapOnHomePage !== 'function') {
    sendResponse(requestId, undefined, 'Snap does not export onHomePage')
    return
  }
  try {
    const result = await snapOnHomePage()
    console.error('XXXZZZ snap_executor handleGetHomePage result:', JSON.stringify(result)?.slice(0, 300))

    // onHomePage may return { content: <tree> } or { id: <interfaceId> }.
    // If it returned { id }, resolve the component tree from the local store.
    if (result && typeof result === 'object') {
      const r = result as Record<string, unknown>
      if (typeof r['id'] === 'string' && !r['content']) {
        const interfaceId = r['id']
        const content = snapInterfaces.get(interfaceId)
        sendResponse(requestId, { content, interfaceId })
        return
      }
    }

    sendResponse(requestId, result)
  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err)
    console.error('XXXZZZ snap_executor handleGetHomePage error:', msg)
    sendResponse(requestId, undefined, msg)
  }
}

async function handleUserInput(
  interfaceId: string,
  event: unknown,
  requestId: string,
): Promise<void> {
  if (typeof snapOnUserInput !== 'function') {
    sendResponse(requestId, undefined, 'Snap does not export onUserInput')
    return
  }
  try {
    // Track InputChangeEvent in interface state so snap_getInterfaceState works.
    const ev = event as Record<string, unknown> | undefined
    if (ev?.type === 'InputChangeEvent' && typeof ev.name === 'string') {
      let state = snapInterfaceStates.get(interfaceId)
      if (!state) {
        state = {}
        snapInterfaceStates.set(interfaceId, state)
      }
      state[ev.name] = String(ev.value ?? '')
    }
    // FormSubmitEvent — merge all form values into interface state.
    if (ev?.type === 'FormSubmitEvent' && typeof ev.value === 'object' && ev.value) {
      let state = snapInterfaceStates.get(interfaceId)
      if (!state) {
        state = {}
        snapInterfaceStates.set(interfaceId, state)
      }
      Object.assign(state, ev.value)
    }

    console.error('XXXZZZ snap_executor handleUserInput id=', interfaceId, 'event=', JSON.stringify(event))
    await snapOnUserInput({ id: interfaceId, event })

    // After onUserInput completes, the snap may have called snap_updateInterface
    // which updated snapInterfaces. Return the current content so the wallet
    // page can re-render.
    const content = snapInterfaces.get(interfaceId)
    sendResponse(requestId, { content })
  } catch (err) {
    const msg = err instanceof Error ? err.message : String(err)
    console.error('XXXZZZ snap_executor handleUserInput error:', msg)
    sendResponse(requestId, undefined, msg)
  }
}

function handleSnapRequestResponse(
  snapReqId: string,
  result: unknown,
  error?: string,
): void {
  const pending = pendingSnapRequests.get(snapReqId)
  if (!pending) {
    return
  }
  pendingSnapRequests.delete(snapReqId)
  if (error) {
    pending.reject(new Error(error))
  } else {
    pending.resolve(result)
  }
}

window.addEventListener('message', (event: MessageEvent) => {
  // Only accept messages from the parent wallet page.
  if (event.source !== window.parent) {
    return
  }

  const data = event.data as Record<string, unknown>
  if (!data || typeof data !== 'object' || typeof data['type'] !== 'string') {
    return
  }

  switch (data['type']) {
    case 'load_snap':
      handleLoadSnap(
        data['snapId'] as string,
        data['requestId'] as string,
      )
      break

    case 'invoke':
      handleInvoke(
        data['method'] as string,
        data['params'],
        (data['origin'] as string) ?? '',
        data['requestId'] as string,
      )
      break

    case 'snap_request_response':
      // Core (C++) responded to a snap.request() call relayed by the bridge.
      handleSnapRequestResponse(
        data['requestId'] as string,
        data['result'],
        data['error'] as string | undefined,
      )
      break

    case 'get_homepage':
      handleGetHomePage(data['requestId'] as string)
      break

    case 'user_input':
      handleUserInput(
        data['interfaceId'] as string,
        data['event'],
        data['requestId'] as string,
      )
      break

    case 'proxy_fetch':
      // Proxy an HTTP GET through this iframe (which has connect-src *).
      // Used by the wallet page which has a strict CSP.
      void (async () => {
        const requestId = data['requestId'] as string
        const url = data['url'] as string
        try {
          const res = await fetch(url)
          const text = await res.text()
          sendToParent({ type: 'response', requestId, result: text })
        } catch (err) {
          const msg = err instanceof Error ? err.message : String(err)
          sendToParent({ type: 'response', requestId, result: undefined, error: msg })
        }
      })()
      break
  }
})
