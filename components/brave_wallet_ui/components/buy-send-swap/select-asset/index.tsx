import * as React from 'react'
import Fuse from 'fuse.js'

import SelectAssetItem from '../select-asset-item'
import { AccountAssetOptionType } from '../../../constants/types'
import { SearchBar } from '../../shared'
import Header from '../select-header'
import { getLocale } from '../../../../common/locale'
// Styled Components
import {
  SelectWrapper,
  SelectScrollSearchContainer
} from '../shared-styles'

export interface Props {
  assets: AccountAssetOptionType[]
  onSelectAsset: (asset: AccountAssetOptionType) => () => void
  onBack: () => void
}

function SelectAsset (props: Props) {
  const { assets, onBack, onSelectAsset } = props

  const fuse = React.useMemo(() => new Fuse(assets, {
    shouldSort: true,
    threshold: 0.45,
    location: 0,
    distance: 100,
    minMatchCharLength: 1,
    keys: [
      { name: 'asset.name', weight: 0.5 },
      { name: 'asset.symbol', weight: 0.5 }
    ]
  }), [assets])

  const [filteredAssetList, setFilteredAssetList] = React.useState<AccountAssetOptionType[]>(assets)

  const filterAssetList = React.useCallback((event: React.ChangeEvent<HTMLInputElement>) => {
    const search = event.target.value
    if (search === '') {
      setFilteredAssetList(assets)
    } else {
      const filteredList = fuse.search(search).map((result: Fuse.FuseResult<AccountAssetOptionType>) => result.item)
      setFilteredAssetList(filteredList)
    }
  }, [fuse, assets])

  return (
    <SelectWrapper>
      <Header title={getLocale('braveWalletSelectAsset')} onBack={onBack} />
      <SearchBar placeholder={getLocale('braveWalletSearchAsset')} action={filterAssetList} />
      <SelectScrollSearchContainer>
        {
          // Temp filtering out erc721 tokens, sending will be handled in a different PR
          filteredAssetList.filter((token) => !token.asset.isErc721).map((asset: AccountAssetOptionType) =>
            <SelectAssetItem
              key={asset.asset.contractAddress}
              asset={asset}
              onSelectAsset={onSelectAsset(asset)}
            />
          )
        }
      </SelectScrollSearchContainer>
    </SelectWrapper>
  )
}

export default SelectAsset
