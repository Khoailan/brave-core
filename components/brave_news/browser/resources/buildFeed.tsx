import {
  Channel,
  FeedItemMetadata,
  Publisher,
  Signal
} from 'gen/brave/components/brave_news/common/brave_news.mojom.m'
import { normal, pickWeighted, project, tossCoin } from './pick'
import { ArticleElements, Elements } from './model'

const blockMinInline = 1
const blockMaxInline = 5
const inlineDiscoveryRatio = 0.25
const specialCardEveryN = 2
const adsToDiscoverRatio = 0.5

const sourceSubscribedMin = 1/1e5
const sourceSubscribedMax = 1

const channelSubscribedMin = 0.01
const channelSubscribedMax = 1
const channelVisitsMin = 0.5;
const channelVisitsMax = 1;

const sourcesVisitsMin = 0.2

export interface Info {
  publishers: { [id: string]: Publisher }
  suggested: Publisher[]
  articles: FeedItemMetadata[]
  signals: { [key: string]: Signal }
  channels: Channel[]
}

export const unvisited = (articles: FeedItemMetadata[], info: Info) => {
  return articles.filter(a => {
    const signal = info.signals[a.url.url]

    // No signal means we haven't visited the article
    if (!signal) return true

    // No source visits!
    return signal.sourceVisits === 0
  })
}

export const getWeight = (article: FeedItemMetadata, { signals }: Info) => {
  const signal = signals[article.url.url]
  if (!signal) return 0

  if (signal.blocked) return 0

  const channelSubscribedWeighting = signal.channelSubscribed
    ? channelSubscribedMax
    : channelSubscribedMin

  return (
    (sourcesVisitsMin + signal.sourceVisits * (1 - sourcesVisitsMin)) *
    signal.popRecency *
    (signal.sourceSubscribed ? sourceSubscribedMax : sourceSubscribedMin) *
    channelSubscribedWeighting
  )
}

export const getChannelWeight = (channel: Channel, { signals }: Info) => {
  const signal = signals[channel.channelName]
  if (!signal) return 0
  if (signal.blocked) return 0

  const subscribedWeighting = signal.channelSubscribed
    ? channelSubscribedMax
    : channelSubscribedMin

  const visitWeighting = project(signal.channelVisits, channelVisitsMin, channelVisitsMax)
  return subscribedWeighting * visitWeighting;
}

const generateBlock = (info: Info, block: { type: 'default' } | { type: 'channel', id: string }): ArticleElements[] => {
  const articles = block.type === 'default'
    ? [...info.articles]
    : info.articles.filter(a => {
      const publisher = info.publishers[a.publisherId]
      return publisher.locales.flatMap(l => l.channels).includes(block.id)
    });

  const elements: ArticleElements[] = []
  const count = project(normal(), blockMinInline, blockMaxInline)

  if (!articles.length) {
    console.error("No articles for block", block)
    return []
  }

  elements.push({
    article: pickWeighted(articles, i => getWeight(i, info)),
    type: 'hero'
  })

  for (let i = 0; i < count && articles.length > 0; ++i) {
    // Note: Only default blocks can have discover cards.
    const discover = tossCoin(inlineDiscoveryRatio)

    // If this is a discover inline card, then we should pick the most popular unvisited article, by popularity
    if (discover) {
      const discoverable = unvisited(articles, info)
      if (discoverable.length) {
        elements.push({
          type: 'inline',
          article: pickWeighted(discoverable, i => i.score),
          isDiscover: true
        })
        continue
      }
    }

    elements.push({
      type: 'inline',
      article: pickWeighted(articles, i => getWeight(i, info)),
      isDiscover: false
    })
  }

  // Remove the articles, so we don't reuse them.
  for (const element of elements) {
    const index = info.articles.indexOf(element.article)
    info.articles.splice(index, 1)
  }

  return elements
}

export const generateCluster = (info: Info, channelOrTopic: string): Elements | undefined => {
  const elements = generateBlock(info, { type: 'channel', id: channelOrTopic })
  if (!elements.length) return

  return {
    type: 'cluster',
    clusterType: { type: 'channel', id: channelOrTopic },
    elements
  }
}

export const generateRandomCluster = (info: Info) => {
  const randomChannel = pickWeighted([...info.channels], c => getChannelWeight(c, info))
  return generateCluster(info, randomChannel.channelName)
}

export const generateAd = (info: Info, iteration: number): Elements | undefined => {
  if (iteration % specialCardEveryN !== 0) {
    return
  }

  return tossCoin(adsToDiscoverRatio) ? { type: 'advert' } : {
    type: 'discover',
    publishers: info.suggested.splice(0, 3)
  }
}

export const generateFeed = (info: Info) => {
  // First step is to filter out articles we're never going to show.
  info.articles = info.articles.filter((a) => getWeight(a, info) > 0)

  let currentStep = 1
  let iteration = 0

  const nextStep = {
    1: 2,
    2: 3,
    3: 4,
    4: 5,
    5: 3
  }

  const steps = {
    1: () => generateBlock(info, { type: 'default' }),
    2: () => generateBlock(info, { type: 'channel', id: 'Top News' }),
    3: () => generateBlock(info, { type: 'default' }),
    4: () => generateRandomCluster(info),
    5: () => generateAd(info, iteration)
  }

  const result: Elements[] = []
  while (info.articles.length !== 0) {
    const generated = steps[currentStep]()
    currentStep = nextStep[currentStep]
    iteration++

    const items = Array.isArray(generated) ? generated : [generated]
    result.push(...items.filter(i => i))
  }
  return result
}
