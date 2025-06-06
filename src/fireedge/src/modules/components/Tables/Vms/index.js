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
import table from '@modules/components/Tables/Vms/table'
import columns from '@modules/components/Tables/Vms/columns'
import row from '@modules/components/Tables/Vms/row'
import actions from '@modules/components/Tables/Vms/actions'
import rowActions from '@modules/components/Tables/Vms/rowActions'

const VmsTable = {
  Table: table,
  Columns: columns,
  Row: row,
  Actions: actions,
  RowActions: rowActions,
}

export default VmsTable
