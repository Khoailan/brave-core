// Copyright (c) 2020 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.

import * as React from 'react'

// Components
import getNTPBrowserAPI from '../../api/background'
import { addNewTopSite, editTopSite } from '../../api/topSites'
import { brandedWallpaperLogoClicked } from '../../api/wallpaper'
import {
  BinanceWidget as Binance, BraveTalkWidget as BraveTalk, Clock, CryptoDotComWidget as CryptoDotCom, EditCards, EditTopSite, GeminiWidget as Gemini, OverrideReadabilityColor, RewardsWidget as Rewards, SearchPromotion
} from '../../components/default'
import BrandedWallpaperLogo from '../../components/default/brandedWallpaper/logo'
import BraveToday, { GetDisplayAdContent } from '../../components/default/braveToday'
import FooterInfo from '../../components/default/footer/footer'
import * as Page from '../../components/default/page'
import { SponsoredImageTooltip } from '../../components/default/rewards'
import { FTXWidget as FTX } from '../../widgets/ftx/components'
import TopSitesGrid from './gridSites'
import SiteRemovalNotification from './notification'
import Stats from './stats'

// Helpers
import {
  fetchCryptoDotComCharts, fetchCryptoDotComLosersGainers, fetchCryptoDotComSupportedPairs, fetchCryptoDotComTickerPrices
} from '../../api/cryptoDotCom'
import { generateQRData } from '../../binance-utils'
import isReadableOnBackground from '../../helpers/colorUtil'
import VisibilityTimer from '../../helpers/visibilityTimer'

// Types
import { getLocale } from '../../../common/locale'
import { GeminiAssetAddress } from '../../actions/gemini_actions'
import currencyData from '../../components/default/binance/data'
import geminiData from '../../components/default/gemini/data'
import { NewTabActions } from '../../constants/new_tab_types'
import { BraveTodayState } from '../../reducers/today'
import { FTXState } from '../../widgets/ftx/ftx_state'

// NTP features
import { MAX_GRID_SIZE } from '../../constants/new_tab_ui'
import Settings, { TabType as SettingsTabType } from './settings'

import { BraveNewsContextProvider } from '../../components/default/braveToday/customize/Context'
import BraveTodayHint from '../../components/default/braveToday/hint'
import GridWidget from './gridWidget'

interface Props {
  newTabData: NewTab.State
  gridSitesData: NewTab.GridSitesState
  todayData: BraveTodayState
  ftx: FTXState
  actions: NewTabActions
  getBraveNewsDisplayAd: GetDisplayAdContent
  saveShowBackgroundImage: (value: boolean) => void
  saveShowToday: (value: boolean) => any
  saveShowRewards: (value: boolean) => void
  saveShowBraveTalk: (value: boolean) => void
  saveShowBinance: (value: boolean) => void
  saveShowGemini: (value: boolean) => void
  saveShowCryptoDotCom: (value: boolean) => void
  saveShowFTX: (value: boolean) => void
  saveBrandedWallpaperOptIn: (value: boolean) => void
  saveSetAllStackWidgets: (value: boolean) => void
  chooseNewCustomBackgroundImage: () => void
  setCustomImageBackground: (selectedBackground: string) => void
  removeCustomImageBackground: (background: string) => void
  setBraveBackground: (selectedBackground: string) => void
  setColorBackground: (color: string, useRandomColor: boolean) => void
}

interface State {
  showSettingsMenu: boolean
  showEditTopSite: boolean
  targetTopSiteForEditing?: NewTab.Site
  backgroundHasLoaded: boolean
  activeSettingsTab: SettingsTabType | null
  isPromptingBraveToday: boolean
  showSearchPromotion: boolean
  forceToHideWidget: boolean
}

function GetBackgroundImageSrc (props: Props) {
  if (!props.newTabData.showBackgroundImage &&
    (!props.newTabData.brandedWallpaper || props.newTabData.brandedWallpaper.isSponsored)) {
    return undefined
  }
  if (props.newTabData.brandedWallpaper) {
    const wallpaperData = props.newTabData.brandedWallpaper
    if (wallpaperData.wallpaperImageUrl) {
      return wallpaperData.wallpaperImageUrl
    }
  }

  if (props.newTabData.backgroundWallpaper?.type === 'image' ||
      props.newTabData.backgroundWallpaper?.type === 'brave') {
    return props.newTabData.backgroundWallpaper.wallpaperImageUrl
  }

  return undefined
}

function GetShouldShowSearchPromotion (props: Props, showSearchPromotion: boolean) {
  if (GetIsShowingBrandedWallpaper(props)) { return false }

  return props.newTabData.searchPromotionEnabled && showSearchPromotion
}

function GetShouldForceToHideWidget (props: Props, showSearchPromotion: boolean) {
  if (!GetShouldShowSearchPromotion(props, showSearchPromotion)) {
    return false
  }

  // Avoid promotion popup and other widgets overlap with narrow window.
  return window.innerWidth < 1000
}

function GetIsShowingBrandedWallpaper (props: Props) {
  const { newTabData } = props
  return !!((newTabData.brandedWallpaper &&
    newTabData.brandedWallpaper.isSponsored))
}

function GetShouldShowBrandedWallpaperNotification (props: Props) {
  return GetIsShowingBrandedWallpaper(props) &&
    !props.newTabData.isBrandedWallpaperNotificationDismissed
}

class NewTabPage extends React.Component<Props, State> {
  state: State = {
    showSettingsMenu: false,
    showEditTopSite: false,
    backgroundHasLoaded: false,
    activeSettingsTab: null,
    isPromptingBraveToday: false,
    showSearchPromotion: false,
    forceToHideWidget: false
  }

  braveNewsPromptTimerId: number
  hasInitBraveToday: boolean = false
  imageSource?: string = undefined
  timerIdForBrandedWallpaperNotification?: number = undefined
  onVisiblityTimerExpired = () => {
    this.dismissBrandedWallpaperNotification(false)
  }

  visibilityTimer = new VisibilityTimer(this.onVisiblityTimerExpired, 4000)

  componentDidMount () {
    // if a notification is open at component mounting time, close it
    this.props.actions.showTilesRemovedNotice(false)
    this.imageSource = GetBackgroundImageSrc(this.props)
    this.trackCachedImage()
    if (GetShouldShowBrandedWallpaperNotification(this.props)) {
      this.trackBrandedWallpaperNotificationAutoDismiss()
    }
    this.checkShouldOpenSettings()
    // Only prompt for brave today if we're not scrolling down.
    const shouldPromptBraveToday =
      this.props.newTabData.featureFlagBraveNewsPromptEnabled &&
      this.props.newTabData.showToday &&
      !this.props.todayData.articleScrollTo
    if (shouldPromptBraveToday) {
      this.braveNewsPromptTimerId = window.setTimeout(() => {
        if (window.scrollY > 0) {
          // Don't do anything if user has already scrolled
          return
        }
        this.setState({ isPromptingBraveToday: true })
      }, 1700)
    }
    const searchPromotionEnabled = this.props.newTabData.searchPromotionEnabled
    this.setState({
      showSearchPromotion: searchPromotionEnabled,
      forceToHideWidget: GetShouldForceToHideWidget(this.props, searchPromotionEnabled)
    })
    window.addEventListener('resize', this.handleResize.bind(this))
  }

  componentWillUnmount () {
    if (this.braveNewsPromptTimerId) {
      window.clearTimeout(this.braveNewsPromptTimerId)
    }
    window.removeEventListener('resize', this.handleResize.bind(this))
  }

  componentDidUpdate (prevProps: Props) {
    const oldImageSource = GetBackgroundImageSrc(prevProps)
    const newImageSource = GetBackgroundImageSrc(this.props)
    this.imageSource = newImageSource
    if (newImageSource && oldImageSource !== newImageSource) {
      this.trackCachedImage()
    }
    if (oldImageSource &&
      !newImageSource) {
      // reset loaded state
      this.setState({ backgroundHasLoaded: false })
    }
    if (!GetShouldShowBrandedWallpaperNotification(prevProps) &&
      GetShouldShowBrandedWallpaperNotification(this.props)) {
      this.trackBrandedWallpaperNotificationAutoDismiss()
    }

    if (GetShouldShowBrandedWallpaperNotification(prevProps) &&
      !GetShouldShowBrandedWallpaperNotification(this.props)) {
      this.stopWaitingForBrandedWallpaperNotificationAutoDismiss()
    }
  }

  shouldOverrideReadabilityColor (newTabData: NewTab.State) {
    return !newTabData.brandedWallpaper && newTabData.backgroundWallpaper?.type === 'color' && !isReadableOnBackground(newTabData.backgroundWallpaper)
  }

  handleResize () {
    this.setState({
      forceToHideWidget: GetShouldForceToHideWidget(this.props, this.state.showSearchPromotion)
    })
  }

  trackCachedImage () {
    if (this.state.backgroundHasLoaded) {
      this.setState({ backgroundHasLoaded: false })
    }
    if (this.imageSource) {
      const imgCache = new Image()
      imgCache.src = this.imageSource
      console.timeStamp('image start loading...')
      imgCache.onload = () => {
        console.timeStamp('image loaded')
        this.setState({
          backgroundHasLoaded: true
        })
      }
    }
  }

  trackBrandedWallpaperNotificationAutoDismiss () {
    // Wait until page has been visible for an uninterupted Y seconds and then
    // dismiss the notification.
    this.visibilityTimer.startTracking()
  }

  checkShouldOpenSettings () {
    const params = window.location.search
    const urlParams = new URLSearchParams(params)
    const openSettings = urlParams.get('openSettings') || this.props.newTabData.forceSettingsTab

    if (openSettings) {
      let activeSettingsTab: SettingsTabType | null = null
      const activeSettingsTabRaw = typeof openSettings === 'string'
        ? openSettings
        : this.props.newTabData.forceSettingsTab || null
      if (activeSettingsTabRaw) {
        const allSettingsTabTypes = [...Object.keys(SettingsTabType)]
        if (allSettingsTabTypes.includes(activeSettingsTabRaw)) {
          activeSettingsTab = SettingsTabType[activeSettingsTabRaw]
        }
      }
      this.setState({ showSettingsMenu: true, activeSettingsTab })
      // Remove settings param so menu doesn't persist on reload
      window.history.pushState(null, '', '/')
    }
  }

  stopWaitingForBrandedWallpaperNotificationAutoDismiss () {
    this.visibilityTimer.stopTracking()
  }

  toggleShowBackgroundImage = () => {
    this.props.saveShowBackgroundImage(
      !this.props.newTabData.showBackgroundImage
    )
  }

  toggleShowToday = () => {
    this.props.saveShowToday(
      !this.props.newTabData.showToday
    )
  }

  toggleShowTopSites = () => {
    const { showTopSites, customLinksEnabled } = this.props.newTabData
    this.props.actions.setMostVisitedSettings(!showTopSites, customLinksEnabled)
  }

  toggleCustomLinksEnabled = () => {
    const { showTopSites, customLinksEnabled } = this.props.newTabData
    this.props.actions.setMostVisitedSettings(showTopSites, !customLinksEnabled)
  }

  setMostVisitedSettings = (showTopSites: boolean, customLinksEnabled: boolean) => {
    this.props.actions.setMostVisitedSettings(showTopSites, customLinksEnabled)
  }

  toggleShowRewards = () => {
    this.props.saveShowRewards(!this.props.newTabData.showRewards)
  }

  toggleShowBraveTalk = () => {
    this.props.saveShowBraveTalk(!this.props.newTabData.showBraveTalk)
  }

  toggleShowBinance = () => {
    const { showBinance, binanceState } = this.props.newTabData

    this.props.saveShowBinance(!showBinance)

    if (!showBinance) {
      return
    }

    if (binanceState.userAuthed) {
      chrome.binance.revokeToken(() => {
        this.disconnectBinance()
      })
    } else {
      this.disconnectBinance()
    }
  }

  toggleShowGemini = () => {
    const { showGemini, geminiState } = this.props.newTabData

    this.props.saveShowGemini(!showGemini)

    if (!showGemini) {
      return
    }

    if (geminiState.userAuthed) {
      chrome.gemini.revokeToken(() => {
        this.disconnectGemini()
      })
    } else {
      this.disconnectGemini()
    }
  }

  toggleShowCryptoDotCom = () => {
    this.props.saveShowCryptoDotCom(!this.props.newTabData.showCryptoDotCom)
  }

  toggleShowFTX = () => {
    this.props.actions.ftx.disconnect()
    this.props.saveShowFTX(!this.props.newTabData.showFTX)
  }

  onBinanceClientUrl = (clientUrl: string) => {
    this.props.actions.onBinanceClientUrl(clientUrl)
  }

  onGeminiClientUrl = (clientUrl: string) => {
    this.props.actions.onGeminiClientUrl(clientUrl)
  }

  onValidBinanceAuthCode = () => {
    this.props.actions.onValidBinanceAuthCode()
  }

  onValidGeminiAuthCode = () => {
    this.props.actions.onValidGeminiAuthCode()
  }

  setBinanceHideBalance = (hide: boolean) => {
    this.props.actions.setBinanceHideBalance(hide)
  }

  setGeminiHideBalance = (hide: boolean) => {
    this.props.actions.setGeminiHideBalance(hide)
  }

  disconnectBinance = () => {
    this.props.actions.disconnectBinance()
  }

  setBinanceDisconnectInProgress = () => {
    this.props.actions.setBinanceDisconnectInProgress(true)
  }

  cancelBinanceDisconnect = () => {
    this.props.actions.setBinanceDisconnectInProgress(false)
  }

  disconnectGemini = () => {
    this.props.actions.disconnectGemini()
  }

  setGeminiDisconnectInProgress = () => {
    this.props.actions.setGeminiDisconnectInProgress(true)
  }

  cancelGeminiDisconnect = () => {
    this.props.actions.setGeminiDisconnectInProgress(false)
  }

  connectBinance = () => {
    this.props.actions.connectToBinance()
  }

  connectGemini = () => {
    this.props.actions.connectToGemini()
  }

  buyCrypto = (coin: string, amount: string, fiat: string) => {
    const { userLocale, userTLD } = this.props.newTabData.binanceState
    const refCode = userTLD === 'us' ? '35089877' : '39346846'
    const refParams = `ref=${refCode}&utm_source=brave`

    if (userTLD === 'us') {
      window.open(`https://www.binance.us/en/buy-sell-crypto?crypto=${coin}&amount=${amount}&${refParams}`, '_blank', 'noopener')
    } else {
      window.open(`https://www.binance.com/${userLocale}/buy-sell-crypto?fiat=${fiat}&crypto=${coin}&amount=${amount}&${refParams}`, '_blank', 'noopener')
    }
  }

  onBinanceUserTLD = (userTLD: NewTab.BinanceTLD) => {
    this.props.actions.onBinanceUserTLD(userTLD)
  }

  onBinanceUserLocale = (userLocale: string) => {
    this.props.actions.onBinanceUserLocale(userLocale)
  }

  setBalanceInfo = (info: Record<string, Record<string, string>>) => {
    this.props.actions.onAssetsBalanceInfo(info)
  }

  setAssetDepositInfo = (symbol: string, address: string, url: string) => {
    this.props.actions.onAssetDepositInfo(symbol, address, url)
  }

  disableBrandedWallpaper = () => {
    this.props.saveBrandedWallpaperOptIn(false)
  }

  toggleShowBrandedWallpaper = () => {
    this.props.saveBrandedWallpaperOptIn(
      !this.props.newTabData.brandedWallpaperOptIn
    )
  }

  startRewards = () => {
    const { rewardsState } = this.props.newTabData
    if (!rewardsState.rewardsEnabled) {
      chrome.braveRewards.openRewardsPanel()
    } else {
      chrome.braveRewards.enableAds()
    }
  }

  dismissBrandedWallpaperNotification = (isUserAction: boolean) => {
    this.props.actions.dismissBrandedWallpaperNotification(isUserAction)
  }

  dismissNotification = (id: string) => {
    this.props.actions.dismissNotification(id)
  }

  closeSettings = () => {
    this.setState({
      showSettingsMenu: false,
      activeSettingsTab: null
    })
  }

  showEditTopSite = (targetTopSiteForEditing?: NewTab.Site) => {
    this.setState({
      showEditTopSite: true,
      targetTopSiteForEditing
    })
  }

  closeEditTopSite = () => {
    this.setState({
      showEditTopSite: false
    })
  }

  closeSearchPromotion = () => {
    this.setState({
      showSearchPromotion: false,
      forceToHideWidget: false
    })
  }

  saveNewTopSite = (title: string, url: string, newUrl: string) => {
    if (url) {
      editTopSite(title, url, newUrl === url ? '' : newUrl)
    } else {
      addNewTopSite(title, newUrl)
    }
    this.closeEditTopSite()
  }

  openSettings = (activeTab?: SettingsTabType) => {
    this.props.actions.customizeClicked()
    this.setState({
      showSettingsMenu: !this.state.showSettingsMenu,
      activeSettingsTab: activeTab || null
    })
  }

  onClickLogo = () => {
    brandedWallpaperLogoClicked(this.props.newTabData.brandedWallpaper)
  }

  openSettingsEditCards = () => {
    this.openSettings(SettingsTabType.Cards)
  }

  setForegroundStackWidget = (widget: NewTab.StackWidget) => {
    this.props.actions.setForegroundStackWidget(widget)
  }

  setInitialAmount = (amount: string) => {
    this.props.actions.setInitialAmount(amount)
  }

  setInitialFiat = (fiat: string) => {
    this.props.actions.setInitialFiat(fiat)
  }

  setInitialAsset = (asset: string) => {
    this.props.actions.setInitialAsset(asset)
  }

  setUserTLDAutoSet = () => {
    this.props.actions.setUserTLDAutoSet()
  }

  learnMoreRewards = () => {
    window.open('https://brave.com/brave-rewards/', '_blank', 'noopener')
  }

  learnMoreBinance = () => [
    window.open('https://brave.com/binance/', '_blank', 'noopener')
  ]

  setAssetDepositQRCodeSrc = (asset: string, src: string) => {
    this.props.actions.onDepositQRForAsset(asset, src)
  }

  setGeminiAssetAddress = (address: string, asset: string, qrCode: string) => {
    const assetAddresses: GeminiAssetAddress[] = [{ asset, address, qrCode }]
    if (asset === 'ETH') {
      // Use ETH's address and qrCode for all other erc tokens.
      geminiData.ercTokens.forEach((ercToken: string) => {
        assetAddresses.push({
          asset: ercToken,
          address,
          qrCode
        })
      })
    }

    this.props.actions.setGeminiAssetAddress(assetAddresses)
  }

  setConvertableAssets = (asset: string, assets: string[]) => {
    this.props.actions.onConvertableAssets(asset, assets)
  }

  setBinanceSelectedView = (view: string) => {
    this.props.actions.setBinanceSelectedView(view)
  }

  setGeminiSelectedView = (view: string) => {
    this.props.actions.setGeminiSelectedView(view)
  }

  setGeminiAuthInvalid = () => {
    this.props.actions.setGeminiAuthInvalid(true)
    this.props.actions.disconnectGemini()
  }

  binanceUpdateActions = () => {
    this.fetchBalance()
    this.getConvertAssets()
  }

  binanceRefreshActions = () => {
    this.fetchBalance()
    this.setDepositInfo()
    this.getConvertAssets()
  }

  geminiUpdateActions = () => {
    this.fetchGeminiTickerPrices()
    this.fetchGeminiBalances()
    this.fetchGeminiDepositInfo()
  }

  fetchGeminiTickerPrices = () => {
    geminiData.currencies.map((asset: string) => {
      chrome.gemini.getTickerPrice(`${asset}usd`, (price: string) => {
        this.props.actions.setGeminiTickerPrice(asset, price)
      })
    })
  }

  onCryptoDotComMarketsRequested = async (assets: string[]) => {
    const [tickerPrices, losersGainers] = await Promise.all([
      fetchCryptoDotComTickerPrices(assets),
      fetchCryptoDotComLosersGainers()
    ])
    this.props.actions.cryptoDotComMarketDataUpdate(tickerPrices, losersGainers)
  }

  onCryptoDotComAssetData = async (assets: string[]) => {
    const [charts, pairs] = await Promise.all([
      fetchCryptoDotComCharts(assets),
      fetchCryptoDotComSupportedPairs()
    ])
    this.props.actions.setCryptoDotComAssetData(charts, pairs)
  }

  cryptoDotComUpdateActions = async () => {
    const { supportedPairs, tickerPrices: prices } = this.props.newTabData.cryptoDotComState
    const assets = Object.keys(prices)
    const supportedPairsSet = Object.keys(supportedPairs).length

    const [tickerPrices, losersGainers, charts] = await Promise.all([
      fetchCryptoDotComTickerPrices(assets),
      fetchCryptoDotComLosersGainers(),
      fetchCryptoDotComCharts(assets)
    ])

    // These are rarely updated, so we only need to fetch them
    // in the refresh interval if they aren't set yet (perhaps due to no connection)
    if (!supportedPairsSet) {
      const pairs = await fetchCryptoDotComSupportedPairs()
      this.props.actions.setCryptoDotComSupportedPairs(pairs)
    }

    this.props.actions.onCryptoDotComRefreshData(tickerPrices, losersGainers, charts)
  }

  onBtcPriceOptIn = async () => {
    this.props.actions.onBtcPriceOptIn()
    this.props.actions.onCryptoDotComInteraction()
    await this.onCryptoDotComMarketsRequested(['BTC'])
  }

  onCryptoDotComBuyCrypto = () => {
    this.props.actions.onCryptoDotComBuyCrypto()
  }

  onCryptoDotComInteraction = () => {
    this.props.actions.onCryptoDotComInteraction()
  }

  onCryptoDotComOptInMarkets = (show: boolean) => {
    this.props.actions.onCryptoDotComOptInMarkets(show)
  }

  fetchGeminiBalances = () => {
    chrome.gemini.getAccountBalances((balances: Record<string, string>, authInvalid: boolean) => {
      if (authInvalid) {
        chrome.gemini.refreshAccessToken((success: boolean) => {
          if (!success) {
            this.setGeminiAuthInvalid()
          }
        })
        return
      }

      this.props.actions.setGeminiAccountBalances(balances)
    })
  }

  updateGeminiAssetAddress = (asset: string, address: string) => {
    generateQRData(address, asset, this.setGeminiAssetAddress.bind(this, address))
  }

  fetchGeminiDepositInfo = () => {
    // Remove all ercTokens and update their address when ETH's address is fetched.
    const toRemove = new Set(geminiData.ercTokens)
    const targetAssets = geminiData.currencies.filter(x => !toRemove.has(x))

    targetAssets.forEach((asset: string) => {
      chrome.gemini.getDepositInfo(`${asset.toLowerCase()}`, (address: string) => {
        if (address) {
          this.updateGeminiAssetAddress(asset, address)
        }
      })
    })
  }

  getCurrencyList = () => {
    const { accountBalances, userTLD } = this.props.newTabData.binanceState
    const { usCurrencies, comCurrencies } = currencyData
    const baseList = userTLD === 'us' ? usCurrencies : comCurrencies

    if (!accountBalances) {
      return baseList
    }

    const accounts = Object.keys(accountBalances)
    const nonHoldingList = baseList.filter((symbol: string) => {
      return !accounts.includes(symbol)
    })

    return accounts.concat(nonHoldingList)
  }

  getConvertAssets = () => {
    chrome.binance.getConvertAssets((assets: any) => {
      for (let asset in assets) {
        if (assets[asset]) {
          this.setConvertableAssets(asset, assets[asset])
        }
      }
    })
  }

  fetchBalance = () => {
    const { depositInfoSaved } = this.props.newTabData.binanceState

    chrome.binance.getAccountBalances((balances: Record<string, Record<string, string>>, success: boolean) => {
      const hasBalances = Object.keys(balances).length

      if (!hasBalances) {
        return
      } else if (!success) {
        this.setAuthInvalid()
        return
      }

      this.setBalanceInfo(balances)

      if (!depositInfoSaved) {
        this.setDepositInfo()
      }
    })
  }

  setDepositInfo = () => {
    chrome.binance.getCoinNetworks((networks: Record<string, string>) => {
      const currencies = this.getCurrencyList()
      for (let ticker in networks) {
        if (currencies.includes(ticker)) {
          chrome.binance.getDepositInfo(ticker, networks[ticker], async (address: string, tag: string) => {
            this.setAssetDepositInfo(ticker, address, tag)
            await generateQRData((tag || address), ticker, this.setAssetDepositQRCodeSrc)
          })
        }
      }
      if (Object.keys(networks).length) {
        this.props.actions.setDepositInfoSaved()
      }
    })
  }

  setAuthInvalid = () => {
    this.props.actions.setAuthInvalid(true)
    this.props.actions.disconnectBinance()
  }

  dismissAuthInvalid = () => {
    this.props.actions.setAuthInvalid(false)
  }

  dismissGeminiAuthInvalid = () => {
    this.props.actions.setGeminiAuthInvalid(false)
  }

  getCryptoContent () {
    if (this.props.newTabData.hideAllWidgets) {
      return null
    }
    const {
      widgetStackOrder,
      braveRewardsSupported,
      braveTalkSupported,
      showRewards,
      showBinance,
      showBraveTalk,
      showGemini,
      geminiSupported,
      showCryptoDotCom,
      cryptoDotComSupported,
      showFTX,
      ftxSupported,
      binanceSupported
    } = this.props.newTabData
    const lookup = {
      'rewards': {
        display: braveRewardsSupported && showRewards,
        render: this.renderRewardsWidget.bind(this)
      },
      'binance': {
        display: binanceSupported && showBinance,
        render: this.renderBinanceWidget.bind(this)
      },
      'braveTalk': {
        display: braveTalkSupported && showBraveTalk,
        render: this.renderBraveTalkWidget.bind(this)
      },
      'gemini': {
        display: showGemini && geminiSupported,
        render: this.renderGeminiWidget.bind(this)
      },
      'cryptoDotCom': {
        display: showCryptoDotCom && cryptoDotComSupported,
        render: this.renderCryptoDotComWidget.bind(this)
      },
      'ftx': {
        display: showFTX && ftxSupported,
        render: this.renderFTXWidget.bind(this)
      }
    }

    const widgetList = widgetStackOrder.filter((widget: NewTab.StackWidget) => {
      if (!lookup.hasOwnProperty(widget)) {
        return false
      }

      return lookup[widget].display
    })

    return (
      <>
        {widgetList.map((widget: NewTab.StackWidget, i: number) => {
          const isForeground = i === widgetList.length - 1
          return (
            <div key={`widget-${widget}`}>
              {lookup[widget].render(isForeground, i)}
            </div>
          )
        })}
      </>
    )
  }

  allWidgetsHidden = () => {
    const {
      braveRewardsSupported,
      braveTalkSupported,
      showRewards,
      showBinance,
      showBraveTalk,
      geminiSupported,
      showGemini,
      showCryptoDotCom,
      cryptoDotComSupported,
      showFTX,
      ftxSupported,
      binanceSupported,
      hideAllWidgets
    } = this.props.newTabData
    return hideAllWidgets || [
      braveRewardsSupported && showRewards,
      braveTalkSupported && showBraveTalk,
      binanceSupported && showBinance,
      geminiSupported && showGemini,
      cryptoDotComSupported && showCryptoDotCom,
      ftxSupported && showFTX
    ].every((widget: boolean) => !widget)
  }

  renderCryptoContent () {
    const { newTabData } = this.props
    const { widgetStackOrder } = newTabData
    const allWidgetsHidden = this.allWidgetsHidden()

    if (!widgetStackOrder.length) {
      return null
    }

    return (
      <Page.GridItemWidgetStack>
        {this.getCryptoContent()}
        {!allWidgetsHidden &&
          <EditCards onEditCards={this.openSettingsEditCards} />
        }
      </Page.GridItemWidgetStack>
    )
  }

  renderSearchPromotion () {
    if (!GetShouldShowSearchPromotion(this.props, this.state.showSearchPromotion)) {
      return null
    }

    const onClose = () => { this.closeSearchPromotion() }
    const onDismiss = () => { getNTPBrowserAPI().pageHandler.dismissBraveSearchPromotion() }
    const onTryBraveSearch = (input: string, openNewTab: boolean) => { getNTPBrowserAPI().pageHandler.tryBraveSearchPromotion(input, openNewTab) }

    return (
      <SearchPromotion textDirection={this.props.newTabData.textDirection} onTryBraveSearch={onTryBraveSearch} onClose={onClose} onDismiss={onDismiss} />
    )
  }

  renderBrandedWallpaperNotification () {
    if (!GetShouldShowBrandedWallpaperNotification(this.props)) {
      return null
    }

    const { rewardsState } = this.props.newTabData
    if (!rewardsState.adsSupported) {
      return null
    }

    const onClose = () => { this.dismissBrandedWallpaperNotification(true) }

    return (
      <Page.BrandedWallpaperNotification>
        <SponsoredImageTooltip
          adsEnabled={rewardsState.enabledAds}
          onEnableAds={this.startRewards}
          onClose={onClose}
        />
      </Page.BrandedWallpaperNotification>
    )
  }

  renderRewardsWidget (showContent: boolean, position: number) {
    const { rewardsState, showRewards, textDirection, braveRewardsSupported } = this.props.newTabData
    if (!braveRewardsSupported || !showRewards) {
      return null
    }

    return (
      <Rewards
        {...rewardsState}
        widgetTitle={getLocale('rewardsWidgetBraveRewards')}
        onLearnMore={this.learnMoreRewards}
        menuPosition={'left'}
        isCrypto={true}
        paddingType={'none'}
        isCryptoTab={!showContent}
        isForeground={showContent}
        stackPosition={position}
        textDirection={textDirection}
        preventFocus={false}
        hideWidget={this.toggleShowRewards}
        showContent={showContent}
        onShowContent={this.setForegroundStackWidget.bind(this, 'rewards')}
        onDismissNotification={this.dismissNotification}
      />
    )
  }

  renderBraveTalkWidget (showContent: boolean, position: number) {
    const { newTabData } = this.props
    const { showBraveTalk, textDirection, braveTalkSupported } = newTabData

    if (!showBraveTalk || !braveTalkSupported) {
      return null
    }

    return (
      <BraveTalk
        isCrypto={true}
        paddingType={'none'}
        menuPosition={'left'}
        widgetTitle={getLocale('braveTalkWidgetTitle')}
        isForeground={showContent}
        stackPosition={position}
        textDirection={textDirection}
        hideWidget={this.toggleShowBraveTalk}
        showContent={showContent}
        onShowContent={this.setForegroundStackWidget.bind(this, 'braveTalk')}
      />
    )
  }

  renderBinanceWidget (showContent: boolean, position: number) {
    const { newTabData } = this.props
    const { binanceState, showBinance, textDirection, binanceSupported } = newTabData

    if (!showBinance || !binanceSupported) {
      return null
    }

    return (
      <Binance
        {...binanceState}
        isCrypto={true}
        paddingType={'none'}
        isCryptoTab={!showContent}
        menuPosition={'left'}
        widgetTitle={'Binance'}
        isForeground={showContent}
        stackPosition={position}
        textDirection={textDirection}
        preventFocus={false}
        hideWidget={this.toggleShowBinance}
        showContent={showContent}
        onSetHideBalance={this.setBinanceHideBalance}
        onBinanceClientUrl={this.onBinanceClientUrl}
        onConnectBinance={this.connectBinance}
        onDisconnectBinance={this.disconnectBinance}
        onCancelDisconnect={this.cancelBinanceDisconnect}
        onValidAuthCode={this.onValidBinanceAuthCode}
        onBuyCrypto={this.buyCrypto}
        onBinanceUserTLD={this.onBinanceUserTLD}
        onBinanceUserLocale={this.onBinanceUserLocale}
        onShowContent={this.setForegroundStackWidget.bind(this, 'binance')}
        onSetInitialAmount={this.setInitialAmount}
        onSetInitialAsset={this.setInitialAsset}
        onSetInitialFiat={this.setInitialFiat}
        onSetUserTLDAutoSet={this.setUserTLDAutoSet}
        onUpdateActions={this.binanceUpdateActions}
        onDismissAuthInvalid={this.dismissAuthInvalid}
        onSetSelectedView={this.setBinanceSelectedView}
        getCurrencyList={this.getCurrencyList}
        onLearnMore={this.learnMoreBinance}
        onRefreshData={binanceState.userAuthed ? this.binanceRefreshActions : undefined}
        onDisconnect={binanceState.userAuthed ? this.setBinanceDisconnectInProgress : undefined}
      />
    )
  }

  renderGeminiWidget (showContent: boolean, position: number) {
    const { newTabData } = this.props
    const { geminiState, showGemini, textDirection, geminiSupported } = newTabData

    if (!showGemini || !geminiSupported) {
      return null
    }

    return (
      <Gemini
        {...geminiState}
        isCrypto={true}
        paddingType={'none'}
        isCryptoTab={!showContent}
        menuPosition={'left'}
        widgetTitle={'Gemini'}
        isForeground={showContent}
        stackPosition={position}
        textDirection={textDirection}
        preventFocus={false}
        hideWidget={this.toggleShowGemini}
        showContent={showContent}
        onShowContent={this.setForegroundStackWidget.bind(this, 'gemini')}
        onDisableWidget={this.toggleShowGemini}
        onValidAuthCode={this.onValidGeminiAuthCode}
        onConnectGemini={this.connectGemini}
        onGeminiClientUrl={this.onGeminiClientUrl}
        onUpdateActions={this.geminiUpdateActions}
        onSetSelectedView={this.setGeminiSelectedView}
        onSetHideBalance={this.setGeminiHideBalance}
        onCancelDisconnect={this.cancelGeminiDisconnect}
        onDisconnectGemini={this.disconnectGemini}
        onDismissAuthInvalid={this.dismissGeminiAuthInvalid}
        onDisconnect={geminiState.userAuthed ? this.setGeminiDisconnectInProgress : undefined}
        onRefreshData={geminiState.userAuthed ? this.geminiUpdateActions : undefined}
      />
    )
  }

  renderCryptoDotComWidget (showContent: boolean, position: number) {
    const { newTabData } = this.props
    const { cryptoDotComState, showCryptoDotCom, textDirection, cryptoDotComSupported } = newTabData

    if (!showCryptoDotCom || !cryptoDotComSupported) {
      return null
    }

    return (
      <CryptoDotCom
        {...cryptoDotComState}
        isCrypto={true}
        paddingType={'none'}
        isCryptoTab={!showContent}
        menuPosition={'left'}
        widgetTitle={'Crypto.com'}
        isForeground={showContent}
        stackPosition={position}
        textDirection={textDirection}
        preventFocus={false}
        hideWidget={this.toggleShowCryptoDotCom}
        showContent={showContent}
        onShowContent={this.setForegroundStackWidget.bind(this, 'cryptoDotCom')}
        onViewMarketsRequested={this.onCryptoDotComMarketsRequested}
        onSetAssetData={this.onCryptoDotComAssetData}
        onUpdateActions={this.cryptoDotComUpdateActions}
        onDisableWidget={this.toggleShowCryptoDotCom}
        onBtcPriceOptIn={this.onBtcPriceOptIn}
        onBuyCrypto={this.onCryptoDotComBuyCrypto}
        onInteraction={this.onCryptoDotComInteraction}
        onOptInMarkets={this.onCryptoDotComOptInMarkets}
      />
    )
  }

  renderFTXWidget (showContent: boolean, position: number) {
    const { newTabData } = this.props
    const { showFTX, textDirection, ftxSupported } = newTabData

    if (!showFTX || !ftxSupported) {
      return null
    }

    return (
      <FTX
        ftx={this.props.ftx}
        actions={this.props.actions.ftx}
        isCrypto={true}
        paddingType={'none'}
        isCryptoTab={!showContent}
        menuPosition={'left'}
        widgetTitle={'FTX'}
        isForeground={showContent}
        stackPosition={position}
        textDirection={textDirection}
        preventFocus={false}
        hideWidget={this.toggleShowFTX}
        showContent={showContent}
        onShowContent={this.setForegroundStackWidget.bind(this, 'ftx')}
        onDisconnect={this.props.ftx.isConnected ? this.props.actions.ftx.disconnect : undefined}
      />
    )
  }

  render () {
    const { newTabData, gridSitesData, actions } = this.props
    const { showSettingsMenu, showEditTopSite, targetTopSiteForEditing, forceToHideWidget } = this.state

    if (!newTabData) {
      return null
    }

    const hasImage = this.imageSource !== undefined
    const isShowingBrandedWallpaper = !!newTabData.brandedWallpaper

    const hasWallpaperInfo = newTabData.backgroundWallpaper?.type === 'brave'
    const colorForBackground = newTabData.backgroundWallpaper?.type === 'color' ? newTabData.backgroundWallpaper.wallpaperColor : undefined

    let cryptoContent = this.renderCryptoContent()
    const showAddNewSiteMenuItem = newTabData.customLinksNum < MAX_GRID_SIZE

    let { showTopSites, showStats, showClock } = newTabData
    // In favorites mode, add site tile is visible by default if there is no
    // item. In frecency, top sites widget is hidden with empty tiles.
    if (showTopSites && !newTabData.customLinksEnabled) {
      showTopSites = this.props.gridSitesData.gridSites.length !== 0
    }

    if (forceToHideWidget) {
      showTopSites = false
      showStats = false
      showClock = false
      cryptoContent = null
    }

    const BraveNewsContext = newTabData.featureFlagBraveNewsV2Enabled
      ? BraveNewsContextProvider
      : React.Fragment

    return (
      <Page.App
        dataIsReady={newTabData.initialDataLoaded}
        hasImage={hasImage}
        imageSrc={this.imageSource}
        imageHasLoaded={this.state.backgroundHasLoaded}
        colorForBackground={colorForBackground}
        data-show-news-prompt={((this.state.backgroundHasLoaded || colorForBackground) && this.state.isPromptingBraveToday) ? true : undefined}>
        <OverrideReadabilityColor override={ this.shouldOverrideReadabilityColor(this.props.newTabData) } />
        <BraveNewsContext>
        <Page.Page
            hasImage={hasImage}
            imageSrc={this.imageSource}
            imageHasLoaded={this.state.backgroundHasLoaded}
            showClock={showClock}
            showStats={showStats}
            colorForBackground={colorForBackground}
            showCryptoContent={!!cryptoContent}
            showTopSites={showTopSites}
            showBrandedWallpaper={isShowingBrandedWallpaper}
        >
          {this.renderSearchPromotion()}
          <GridWidget
            pref='showStats'
            container={Page.GridItemStats}
            paddingType={'right'}
            widgetTitle={getLocale('statsTitle')}
            textDirection={newTabData.textDirection}
            menuPosition={'right'}>
            <Stats stats={newTabData.stats}/>
          </GridWidget>
          <GridWidget
            pref='showClock'
            container={Page.GridItemClock}
            paddingType='right'
            widgetTitle={getLocale('clockTitle')}
            textDirection={newTabData.textDirection}
            menuPosition='left'>
            <Clock />
          </GridWidget>
          {
            showTopSites &&
              <Page.GridItemTopSites>
                <TopSitesGrid
                  actions={actions}
                  paddingType={'none'}
                  customLinksEnabled={newTabData.customLinksEnabled}
                  onShowEditTopSite={this.showEditTopSite}
                  widgetTitle={getLocale('topSitesTitle')}
                  gridSites={gridSitesData.gridSites}
                  menuPosition={'right'}
                  hideWidget={this.toggleShowTopSites}
                  onAddSite={showAddNewSiteMenuItem ? this.showEditTopSite : undefined}
                  onToggleCustomLinksEnabled={this.toggleCustomLinksEnabled}
                  textDirection={newTabData.textDirection}
                />
              </Page.GridItemTopSites>
            }
            {
              gridSitesData.shouldShowSiteRemovedNotification
                ? (
                  <Page.GridItemNotification>
                    <SiteRemovalNotification actions={actions} showRestoreAll={!newTabData.customLinksEnabled} />
                  </Page.GridItemNotification>
                ) : null
            }
            {cryptoContent}
            <Page.Footer>
              <Page.FooterContent>
                {isShowingBrandedWallpaper && newTabData.brandedWallpaper &&
                  newTabData.brandedWallpaper.logo &&
                  <Page.GridItemBrandedLogo>
                    <BrandedWallpaperLogo
                      menuPosition={'right'}
                      paddingType={'default'}
                      textDirection={newTabData.textDirection}
                      onClickLogo={this.onClickLogo}
                      data={newTabData.brandedWallpaper.logo}
                    />
                    {this.renderBrandedWallpaperNotification()}
                  </Page.GridItemBrandedLogo>}
                <FooterInfo
                  textDirection={newTabData.textDirection}
                  supportsBraveTalk={newTabData.braveTalkSupported}
                  showBraveTalkPrompt={newTabData.braveTalkPromptAllowed && !newTabData.braveTalkPromptDismissed}
                  backgroundImageInfo={newTabData.backgroundWallpaper}
                  showPhotoInfo={!isShowingBrandedWallpaper && hasWallpaperInfo && newTabData.showBackgroundImage}
                  onClickSettings={this.openSettings}
                  onDismissBraveTalkPrompt={this.props.actions.dismissBraveTalkPrompt}
                />
              </Page.FooterContent>
            </Page.Footer>
            {newTabData.showToday &&
              <Page.GridItemNavigationBraveToday>
                <BraveTodayHint />
              </Page.GridItemNavigationBraveToday>
            }
          </Page.Page>
        { newTabData.showToday && newTabData.featureFlagBraveNewsEnabled &&
        <BraveToday
          feed={this.props.todayData.feed}
          articleToScrollTo={this.props.todayData.articleScrollTo}
          displayAdToScrollTo={this.props.todayData.displayAdToScrollTo}
          displayedPageCount={this.props.todayData.currentPageIndex}
          publishers={this.props.todayData.publishers}
          isFetching={this.props.todayData.isFetching === true}
          hasInteracted={this.props.todayData.hasInteracted}
          isPrompting={this.state.isPromptingBraveToday}
          isUpdateAvailable={this.props.todayData.isUpdateAvailable}
          isOptedIn={this.props.newTabData.isBraveTodayOptedIn}
          onRefresh={this.props.actions.today.refresh}
          onAnotherPageNeeded={this.props.actions.today.anotherPageNeeded}
          onDisable={this.toggleShowToday}
          onFeedItemViewedCountChanged={this.props.actions.today.feedItemViewedCountChanged}
          onCustomizeBraveToday={() => { this.openSettings(SettingsTabType.BraveToday) }}
          onReadFeedItem={this.props.actions.today.readFeedItem}
          onPromotedItemViewed={this.props.actions.today.promotedItemViewed}
          onSetPublisherPref={this.props.actions.today.setPublisherPref}
          onCheckForUpdate={this.props.actions.today.checkForUpdate}
          onOptIn={this.props.actions.today.optIn}
          onViewedDisplayAd={this.props.actions.today.displayAdViewed}
          onVisitDisplayAd={this.props.actions.today.visitDisplayAd}
          getDisplayAd={this.props.getBraveNewsDisplayAd}
        />
        }
        <Settings
          actions={actions}
          textDirection={newTabData.textDirection}
          showSettingsMenu={showSettingsMenu}
          featureFlagBraveNewsEnabled={newTabData.featureFlagBraveNewsEnabled}
          featureCustomBackgroundEnabled={newTabData.featureCustomBackgroundEnabled}
          onClose={this.closeSettings}
          setActiveTab={this.state.activeSettingsTab || undefined}
          onDisplayTodaySection={this.props.actions.today.ensureSettingsData}
          onClearTodayPrefs={this.props.actions.today.resetTodayPrefsToDefault}
          toggleShowBackgroundImage={this.toggleShowBackgroundImage}
          toggleShowToday={this.toggleShowToday}
          toggleShowTopSites={this.toggleShowTopSites}
          setMostVisitedSettings={this.setMostVisitedSettings}
          toggleBrandedWallpaperOptIn={this.toggleShowBrandedWallpaper}
          chooseNewCustomImageBackground={this.props.chooseNewCustomBackgroundImage}
          setCustomImageBackground={this.props.setCustomImageBackground}
          removeCustomImageBackground={this.props.removeCustomImageBackground}
          setBraveBackground={this.props.setBraveBackground}
          setColorBackground={this.props.setColorBackground}
          showBackgroundImage={newTabData.showBackgroundImage}
          showToday={newTabData.showToday}
          showTopSites={newTabData.showTopSites}
          customLinksEnabled={newTabData.customLinksEnabled}
          showRewards={newTabData.showRewards}
          braveRewardsSupported={newTabData.braveRewardsSupported}
          showBinance={newTabData.showBinance}
          brandedWallpaperOptIn={newTabData.brandedWallpaperOptIn}
          allowSponsoredWallpaperUI={newTabData.featureFlagBraveNTPSponsoredImagesWallpaper}
          toggleShowRewards={this.toggleShowRewards}
          toggleShowBinance={this.toggleShowBinance}
          binanceSupported={newTabData.binanceSupported}
          braveTalkSupported={newTabData.braveTalkSupported}
          toggleShowBraveTalk={this.toggleShowBraveTalk}
          showBraveTalk={newTabData.showBraveTalk}
          geminiSupported={newTabData.geminiSupported}
          toggleShowGemini={this.toggleShowGemini}
          showGemini={newTabData.showGemini}
          cryptoDotComSupported={newTabData.cryptoDotComSupported}
          toggleShowCryptoDotCom={this.toggleShowCryptoDotCom}
          showCryptoDotCom={newTabData.showCryptoDotCom}
          ftxSupported={newTabData.ftxSupported}
          toggleShowFTX={this.toggleShowFTX}
          showFTX={newTabData.showFTX}
          todayPublishers={this.props.todayData.publishers}
          cardsHidden={this.allWidgetsHidden()}
          toggleCards={this.props.saveSetAllStackWidgets}
          newTabData={this.props.newTabData}
          onEnableRewards={this.startRewards}
        />
        {
          showEditTopSite
            ? <EditTopSite
              targetTopSiteForEditing={targetTopSiteForEditing}
              textDirection={newTabData.textDirection}
              onClose={this.closeEditTopSite}
              onSave={this.saveNewTopSite}
            /> : null
        }
        </BraveNewsContext>
      </Page.App>
    )
  }
}

export default NewTabPage
