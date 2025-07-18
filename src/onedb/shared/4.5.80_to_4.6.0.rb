# -------------------------------------------------------------------------- #
# Copyright 2019-2025, OpenNebula Systems S.L.                               #
#                                                                            #
# Licensed under the OpenNebula Software License                             #
# (the "License"); you may not use this file except in compliance with       #
# the License. You may obtain a copy of the License as part of the software  #
# distribution.                                                              #
#                                                                            #
# See https://github.com/OpenNebula/one/blob/master/LICENSE.onsla            #
# (or copy bundled with OpenNebula in /usr/share/doc/one/).                  #
#                                                                            #
# Unless agreed to in writing, software distributed under the License is     #
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY   #
# KIND, either express or implied. See the License for the specific language #
# governing permissions and  limitations under the License.                  #
# -------------------------------------------------------------------------- #

require 'nokogiri'

module Migrator
    def db_version
        "4.6.0"
    end

    def one_version
        "OpenNebula 4.6.0"
    end

    def up

        found = false

        @db.fetch("SELECT oid FROM acl WHERE user=17179869184 and resource=140754668224512 and rights=1 and zone=17179869184") do |row|
            found = true
        end

        if !found
            last_oid = -1

            @db.fetch("SELECT last_oid FROM pool_control WHERE tablename='acl'") do |row|
                last_oid = row[:last_oid].to_i
            end

            # * ZONE/* USE *
            @db.run "INSERT INTO acl VALUES(#{last_oid+1},17179869184,140754668224512,1,17179869184);"

            @db.run "REPLACE INTO pool_control VALUES('acl', #{last_oid+1});"
        end

        return true
    end
end
