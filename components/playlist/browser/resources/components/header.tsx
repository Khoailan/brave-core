// Copyright (c) 2023 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import * as React from 'react'

import styled  from 'styled-components'
import { Link } from 'react-router-dom'

import Icon from '@brave/leo/react/icon'
import { color, font } from '@brave/leo/tokens/css'

import * as PlaylistMojo from 'gen/brave/components/playlist/common/mojom/playlist.mojom.m.js'
import PlaylistInfo from './playlistInfo'

const StyledLink = styled(Link)<{}>`
  text-decoration: none;
  color:unset
`

interface HeaderProps {
  playlist?: PlaylistMojo.Playlist
}

const GradientIcon = styled(Icon)<{}>`
 --leo-icon-color: linear-gradient(314.42deg, #FA7250 8.49%, #FF1893 43.72%, #A78AFF 99.51%);;
`

const ColoredIcon = styled(Icon)<{ colorGetter: (colorSet: any) => any }>`
  @media (prefers-color-scheme: light) {
    color: ${({colorGetter}) => colorGetter(color.light)}
  }

  @media (prefers-color-scheme: dark) {
    color: ${({colorGetter}) => colorGetter(color.dark)}
  }
`

const ProductNameContainer = styled.div<{}>`
  display: flex;
  gap: 4px;
  font: ${font.desktop.heading.h4};
`

const ColoredSpan = styled.span<{ colorGetter: (colorSet: any) => any }>`
  @media (prefers-color-scheme: light) {
    color: ${({colorGetter}) => colorGetter(color.light)}
  }

  @media (prefers-color-scheme: dark) {
    color: ${({colorGetter}) => colorGetter(color.dark)}
  }
`

const HeaderContainer = styled.div<{}>`
  display: flex;
  flex-direction: row;
  align-items: center;
  padding: 16px;
  gap: 16px;
  height: 100%;

  @media (prefers-color-scheme: light) {
    border-bottom: 1px solid ${color.light.divider.subtle};
    background: ${color.light.container.background};
  }

  @media (prefers-color-scheme: dark) {
    border-bottom: 1px solid ${color.dark.divider.subtle};
    background: ${color.dark.container.background};
  }
`

export default function Header ({playlist} : HeaderProps) {
  return (
    <HeaderContainer>
      { playlist && 
        <>
          <StyledLink to="/">
            <ColoredIcon name="arrow-left" colorGetter={ colorSet => colorSet.icon.default }/> 
          </StyledLink>
          <PlaylistInfo isDefaultPlaylist={playlist.id === 'default'}
                        itemCount={playlist.items.length}
                        playlistName={playlist.name}
                        totalDuration={0} />
        </>
      }
      { !playlist &&
        <>
          <GradientIcon name="product-playlist-bold-add" /> 
          <ProductNameContainer>
              <ColoredSpan colorGetter={ colorSet =>  colorSet.text.secondary }>Brave</ColoredSpan>
              <ColoredSpan colorGetter={ colorSet => colorSet.text.primary }>Playlist</ColoredSpan>
          </ProductNameContainer>
        </>
      }
    </HeaderContainer>
  )
}
