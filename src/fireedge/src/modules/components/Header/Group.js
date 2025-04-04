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
import { useMemo, memo, JSXElementConstructor } from 'react'
import PropTypes from 'prop-types'

import { Stack, Button, Typography, CircularProgress } from '@mui/material'
import { Group as GroupIcon, VerifiedUser, Check } from 'iconoir-react'

import { useAuth, AuthAPI } from '@FeaturesModule'
import Search from '@modules/components/Search'
import HeaderPopover from '@modules/components/Header/Popover'
import { Tr, Translate } from '@modules/components/HOC'
import { T, FILTER_POOL } from '@ConstantsModule'

const {
  ALL_RESOURCES,
  PRIMARY_GROUP_RESOURCES,
  USER_RESOURCES,
  USER_GROUPS_RESOURCES,
} = FILTER_POOL

const ButtonGroup = memo(
  ({ group, handleClick, disabled }) => {
    const { user, filterPool } = useAuth()
    const { ID, NAME } = group

    const isPrimaryGroup = user?.GID === ID

    const isSelected =
      (filterPool === ALL_RESOURCES && ALL_RESOURCES === ID) ||
      (filterPool === USER_RESOURCES && USER_RESOURCES === ID) ||
      (filterPool === USER_GROUPS_RESOURCES && USER_GROUPS_RESOURCES === ID) ||
      (filterPool === PRIMARY_GROUP_RESOURCES && isPrimaryGroup)

    return (
      <Button
        fullWidth
        disabled={disabled}
        variant={isSelected ? 'contained' : 'outlined'}
        data-cy={`group-${ID}`}
        onClick={handleClick}
        sx={{
          boxShadow: 'none !important',
          justifyContent: 'start',
          '& svg:first-of-type': { my: 0, mx: 2 },
        }}
      >
        <Stack component="span" direction="row" alignItems="center" gap=".5em">
          {isSelected && <Check style={{ margin: 0 }} />}
          <Typography component="span" noWrap variant="subtitle2">
            {NAME}
          </Typography>
          {isPrimaryGroup && <VerifiedUser />}
        </Stack>
      </Button>
    )
  },
  (prev, next) =>
    prev.group.ID === next.group.ID && prev.disabled === next.disabled
)

/**
 * Menu to select the user group that
 * will be used to filter the resources.
 *
 * @returns {JSXElementConstructor} Returns group list
 */
const Group = () => {
  const [changeGroup, { isLoading }] = AuthAPI.useChangeAuthGroupMutation()
  const { user, groups } = useAuth()

  const sortGroupAsMainFirst = (a, b) =>
    a.ID === user?.GUID ? -1 : b.ID === user?.GUID ? 1 : 0

  const ShowAllOption = { ID: ALL_RESOURCES, NAME: Tr(T.ShowAll) }
  const ShowAllOptionUserAndGroups = {
    ID: USER_GROUPS_RESOURCES,
    NAME: Tr(T.ShowBelongingUserAndGroups),
  }
  const ShowAllOptionUser = {
    ID: USER_RESOURCES,
    NAME: Tr(T.ShowBelongingUser),
  }

  const sortMainGroupFirst = useMemo(
    () =>
      [ShowAllOption, ShowAllOptionUserAndGroups, ShowAllOptionUser]
        .concat(groups)
        .sort(sortGroupAsMainFirst),
    [user?.GUID, ShowAllOption?.NAME]
  )

  return (
    <HeaderPopover
      id="group-list"
      icon={<GroupIcon />}
      tooltip={<Translate word={T.SwitchGroup} />}
      buttonProps={{ 'data-cy': 'header-group-button', noborder: true }}
      headerTitle={
        <Stack direction="row" alignItems="center" gap="1em" component="span">
          <Translate word={T.SwitchGroup} />
          {isLoading && <CircularProgress size={20} />}
        </Stack>
      }
    >
      {({ handleClose }) => (
        <Search
          list={sortMainGroupFirst}
          listOptions={{
            shouldSort: true,
            sortFn: sortGroupAsMainFirst,
            keys: ['NAME'],
          }}
          maxResults={5}
          renderResult={(group) => (
            <ButtonGroup
              key={`switcher-group-${group?.ID}`}
              group={group}
              disabled={isLoading}
              handleClick={async () => {
                group?.ID && (await changeGroup({ group: group.ID }))
                handleClose()
              }}
            />
          )}
        />
      )}
    </HeaderPopover>
  )
}

ButtonGroup.propTypes = {
  group: PropTypes.object,
  handleClick: PropTypes.func,
  disabled: PropTypes.bool,
}

ButtonGroup.displayName = 'ButtonGroup'

export default Group
