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

/**
 * Upper case the first character of an input string.
 *
 * @param {string} input - Input string
 * @returns {string} Input string modified
 */
export const upperCaseFirst = (input) =>
  input?.charAt(0)?.toUpperCase() + input.substring(1)

/**
 * Transform into a lower case with spaces between words, then capitalize the string.
 *
 * @param {string} input - String to transform
 * @returns {string} Sentence
 * @example // "test-string" => "Test string"
 * @example // "test_string" => "Test string"
 * @example // "testString" => "Test string"
 * @example // "TESTString" => "Test string"
 */
export const sentenceCase = (input) => {
  const sentence = input
    ?.replace(/[-_]([A-Za-z])/g, ' $1')
    ?.replace(/([A-Z])([A-Z][a-z])/g, '$1 $2')
    ?.replace(/([a-z])([A-Z])/g, '$1 $2')
    ?.toLowerCase()

  return upperCaseFirst(sentence)
}

/**
 * Transform into a string with the separator denoted by the next word capitalized.
 *
 * @param {string} input - String to transform
 * @returns {string} string
 * @example // "test-string" => "testString"
 * @example // "test_string" => "testString"
 */
export const camelCase = (input) =>
  input
    .toLowerCase()
    .replace(/([-_\s][a-z])/gi, ($1) => $1.toUpperCase().replace(/[-_\s]/g, ''))

/**
 * Transform into a snake case string.
 *
 * @param {string} input - String to transform
 * @returns {string} string
 * @example // "test-string" => "test_string"
 * @example // "testString" => "test_string"
 * @example // "TESTString" => "test_string"
 */
export const toSnakeCase = (input) => sentenceCase(input).replace(/\s/g, '_')

/**
 * Converts the string value into a boolean.
 *
 * @param {string} str - String value.
 * @returns {boolean} - If str is "yes" or 1 then returns true,
 * in other cases, return false.
 */
export const stringToBoolean = (str) =>
  ['yes', 'true'].includes(String(str).toLowerCase()) || +str === 1

/**
 * Replace < for &lt; and > for &gt;.
 *
 * @param {string} xmlString - The string with xml value
 * @returns {string} - A string with the same value but with the replace characters
 */
export const transformXmlString = (xmlString = '') =>
  xmlString.replace(/[<>&"']/g, function (c) {
    switch (c) {
      case '<':
        return '&lt;'
      case '>':
        return '&gt;'
      case '&':
        return '&amp;'
      default:
        return c
    }
  })
