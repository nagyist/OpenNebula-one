/* ------------------------------------------------------------------------- *
 * Copyright 2002-2025, OpenNebula Project, OpenNebula Systems               *
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may   *
 * not use this file except in compliance with the License. You may obtain   *
 * a copy of the License at                                                  *
 *                                                                           *
 * http://www.apache.org/licenses/LICENSE-2.0                                *
 *                                                                           *
 * Unless required by applicable law or agreed to in writing, software       *
 * distributed under the License is distributed on an "AS IS" BASIS,         *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  *
 * See the License for the specific language governing permissions and       *
 * limitations under the License.                                            *
 * ------------------------------------------------------------------------- */
import PropTypes from 'prop-types'
import { memo, useCallback, useMemo } from 'react'

import { MarketplaceAppAPI, oneApi } from '@FeaturesModule'
import { MarketplaceAppCard } from '@modules/components/Cards'
import { jsonToXml } from '@ModelsModule'

const Row = memo(
  ({
    original,
    value,
    onClickLabel,
    headerList,
    rowDataCy,
    isSelected,
    toggleRowSelected,
    ...props
  }) => {
    const [update] = MarketplaceAppAPI.useUpdateAppMutation()

    const state = oneApi.endpoints.getMarketplaceApps.useQueryState(undefined, {
      selectFromResult: ({ data = [] }) =>
        data.find((app) => +app.ID === +original.ID),
    })

    const memoApp = useMemo(() => state ?? original, [state, original])

    const handleDeleteLabel = useCallback(
      (label) => {
        const currentLabels = memoApp.TEMPLATE?.LABELS?.split(',')
        const newLabels = currentLabels.filter((l) => l !== label).join(',')
        const newUserTemplate = { ...memoApp.TEMPLATE, LABELS: newLabels }
        const templateXml = jsonToXml(newUserTemplate)

        update({ id: original.ID, template: templateXml, replace: 0 })
      },
      [memoApp.TEMPLATE?.LABELS, update]
    )

    return (
      <MarketplaceAppCard
        app={memoApp}
        rootProps={props}
        onClickLabel={onClickLabel}
        onDeleteLabel={handleDeleteLabel}
      />
    )
  },
  (prev, next) => prev.className === next.className
)

Row.propTypes = {
  original: PropTypes.object,
  value: PropTypes.object,
  isSelected: PropTypes.bool,
  className: PropTypes.string,
  onClick: PropTypes.func,
  onClickLabel: PropTypes.func,
  headerList: PropTypes.oneOfType([PropTypes.array, PropTypes.bool]),
  rowDataCy: PropTypes.string,
  toggleRowSelected: PropTypes.func,
}

Row.displayName = 'MarketplaceAppRow'

export default Row
