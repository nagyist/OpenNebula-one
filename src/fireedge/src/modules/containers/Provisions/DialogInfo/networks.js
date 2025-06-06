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
import { memo, useEffect, useState } from 'react'
import PropTypes from 'prop-types'

import { Trash as DeleteIcon, AddCircledOutline } from 'iconoir-react'
import { Stack, TextField } from '@mui/material'

import { useGeneralApi, ProvisionAPI } from '@FeaturesModule'

import {
  VnsTable,
  NetworkCard,
  Translate,
  SubmitButton,
} from '@ComponentsModule'
import { T } from '@ConstantsModule'

export const Networks = memo(({ id }) => {
  const [amount, setAmount] = useState(() => 1)
  const { enqueueSuccess } = useGeneralApi()

  const [addIp, { isLoading: loadingAddIp, isSuccess: successAddIp }] =
    ProvisionAPI.useAddIpToProvisionMutation()
  const [
    removeResource,
    {
      isLoading: loadingRemove,
      isSuccess: successRemove,
      originalArgs: { id: vnetId } = {},
    },
  ] = ProvisionAPI.useRemoveResourceMutation()
  const { data = {} } = ProvisionAPI.useGetProvisionQuery(id)

  const provisionNetworks =
    data?.TEMPLATE?.BODY?.provision?.infrastructure?.networks?.map(
      (network) => +network.id
    ) ?? []

  useEffect(() => {
    successAddIp && enqueueSuccess(T.SuccessIPAdded, amount)
  }, [successAddIp])

  useEffect(() => {
    successRemove && enqueueSuccess(T.SuccessNetworkDeleted, vnetId)
  }, [successRemove])

  return (
    <>
      <Stack direction="row" mb="0.5em">
        <TextField
          type="number"
          inputProps={{ 'data-cy': 'amount' }}
          onChange={(event) => {
            const newAmount = event.target.value
            ;+newAmount > 0 && setAmount(newAmount)
          }}
          value={amount}
        />
        <Stack alignSelf="center">
          <SubmitButton
            data-cy="add-ip"
            endicon={<AddCircledOutline />}
            label={<Translate word={T.AddIP} />}
            sx={{ ml: 1, display: 'flex', alignItems: 'flex-start' }}
            isSubmitting={loadingAddIp}
            onClick={async () => await addIp({ id, amount })}
          />
        </Stack>
      </Stack>
      <VnsTable.Table
        disableGlobalSort
        disableRowSelect
        displaySelectedRows
        pageSize={5}
        useQuery={() =>
          ProvisionAPI.useGetProvisionResourceQuery(
            /* eslint-disable jsdoc/require-jsdoc */
            { resource: 'network' },
            {
              selectFromResult: ({ data: result = [], ...rest }) => ({
                data: result?.filter((vnet) =>
                  provisionNetworks.includes(+vnet.ID)
                ),
                ...rest,
              }),
            }
          )
        }
        RowComponent={({ original: vnet, handleClick: _, ...props }) => (
          <NetworkCard
            network={vnet}
            rootProps={props}
            actions={
              <>
                <SubmitButton
                  data-cy={`provision-vnet-delete-${vnet.ID}`}
                  icon={<DeleteIcon />}
                  isSubmitting={loadingRemove}
                  onClick={async () =>
                    await removeResource({
                      provision: id,
                      id: vnet.ID,
                      resource: 'network',
                    })
                  }
                />
              </>
            }
          />
        )}
      />
    </>
  )
})

Networks.propTypes = { id: PropTypes.string.isRequired }
Networks.displayName = 'Networks'
