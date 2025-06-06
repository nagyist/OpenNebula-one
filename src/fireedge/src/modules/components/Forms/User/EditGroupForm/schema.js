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
import { array, object, string } from 'yup'

import { INPUT_TYPES, T } from '@ConstantsModule'
import { GroupsTable } from '@modules/components/Tables'
import { getValidationFromFields } from '@UtilsModule'

const GROUPS = (props) => ({
  name: 'groups',
  label: T['user.actions.edit.group.form'],
  type: INPUT_TYPES.TABLE,
  Table: () => GroupsTable.Table,
  fieldProps: {
    filterData: props.filterData,
    preserveState: true,
    singleSelect: props.singleSelect,
  },
  singleSelect: false,
  validation: array(string())
    .required()
    .default(() => undefined),
  grid: { md: 12 },
})

/**
 * Fields of the form.
 *
 * @param {object} props - Object to get filterData function
 * @returns {object} Fields
 */
export const FIELDS = (props) => [GROUPS(props)]

/**
 * Schema of the form.
 *
 * @param {object} props - Object to get filterData function
 * @returns {object} Schema
 */
export const SCHEMA = (props) => object(getValidationFromFields(FIELDS(props)))
