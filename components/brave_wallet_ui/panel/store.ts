/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import { combineReducers, configureStore } from '@reduxjs/toolkit'
import { setupListeners } from '@reduxjs/toolkit/query/react'
import { persistStore } from 'redux-persist'

// handlers
import walletPanelAsyncHandler from './async/wallet_panel_async_handler'
import walletAsyncHandler from '../common/async/handlers'

// api
import getWalletPanelApiProxy from './wallet_panel_api_proxy'

// reducers
import { persistedUiReducer } from '../common/slices/ui.slice'
import {
  walletApiReducer,
  walletApi
} from '../common/slices/api.slice'

import { persistedWalletReducer } from '../common/slices/wallet.slice'
import { panelReducer } from './reducers/panel_reducer'

const combinedReducer = combineReducers({
  panel: panelReducer,
  wallet: persistedWalletReducer,
  ui: persistedUiReducer,
  [walletApi.reducerPath]: walletApiReducer
})

// utils
import { setApiProxyFetcher } from '../common/async/base-query-cache'

const store = configureStore({
  reducer: combinedReducer,
  middleware: (getDefaultMiddleware) => getDefaultMiddleware({
    serializableCheck: false
  }).concat(
    walletAsyncHandler,
    walletPanelAsyncHandler,
    walletApi.middleware
  )
})

export type PanelRootState = ReturnType<typeof combinedReducer>
export type RootStoreState = PanelRootState

const proxy = getWalletPanelApiProxy()
proxy.addJsonRpcServiceObserver(store)
proxy.addKeyringServiceObserver(store)
proxy.addTxServiceObserver(store)
proxy.addBraveWalletServiceObserver(store)

// use this proxy in the api slice of the store
setApiProxyFetcher(getWalletPanelApiProxy)

// enables refetchOnMount and refetchOnReconnect behaviors
// without this, cached data will not refresh when reloading the app
setupListeners(store.dispatch)

export const walletPanelApiProxy = proxy

export const persistor = persistStore(store)

export default store
