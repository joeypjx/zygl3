这是一个基于您提供的文档内容生成的结构化 Markdown 文档。根据您的要求，所有的响应体（以及请求参数）均已整理为表格形式，以确保清晰度和逻辑性。

---

# 接口文档 20251103

**文档说明：** 本文档包含板卡管理、业务链路控制及异常上报等相关接口定义。

---

## 1. 获取所有板卡信息和状态

### 1.1 需求描述
获取所有板卡信息和状态。
* [cite_start]**板卡信息**：机箱号、槽位号、IP地址、板卡类型等 [cite: 1]。
* [cite_start]**板卡状态**：运行状态及正在运行的组件任务信息（任务ID、状态、所属组件、业务链路等） [cite: 1]。

### 1.2 接口基本信息
* **接口名称**：获取所有板卡信息和状态
* **请求方式**：`GET`
* **请求地址**：`http://api/v1/external/qyw/boardinfo`
* **Content-Type**：`application/json`
* [cite_start]**认证方式**：Bearer Token（待定） [cite: 1]

### 1.3 请求头 (Headers)
| 参数名 | 必填 | 示例值 | 说明 |
| :--- | :--- | :--- | :--- |
| Content-Type | 是 | application/json | 声明请求体为 JSON |

### 1.4 响应体 (Response)

| 字段层级 | 字段名 | 类型 | 说明 |
| :--- | :--- | :--- | :--- |
| Root | code | Integer | 响应码（0表示成功） |
| Root | message | String | 响应信息 |
| data[] | chassisName | String | [cite_start]机箱名称 [cite: 1] |
| data[] | chassisNumber | Integer | [cite_start]机箱号 [cite: 1] |
| data[] | boardName | String | [cite_start]板卡名称 [cite: 1] |
| data[] | boardNumber | Integer | [cite_start]板卡槽位号 [cite: 1] |
| data[] | boardType | Integer | [cite_start]板卡类型（见附录） [cite: 2] |
| data[] | boardAddress | String | [cite_start]板卡IP地址 [cite: 2] |
| data[] | boardStatus | Integer | [cite_start]板卡状态：0-正常，1-异常 [cite: 2] |
| data[] | voltage | String/Float | [cite_start]电压 [cite: 2] |
| data[] | current | String/Float | [cite_start]电流 [cite: 3] |
| data[] | temperature | String/Float | [cite_start]温度 [cite: 3] |
| data[].fanSpeeds[] | fanName | String | [cite_start]风扇名称 [cite: 3] |
| data[].fanSpeeds[] | speed | String/Int | [cite_start]风扇转速 [cite: 3] |
| data[].taskInfos[] | taskID | String | [cite_start]任务ID [cite: 3] |
| data[].taskInfos[] | taskStatus | Integer | [cite_start]任务状态：1-运行中, 2-已完成, 3-异常, 0-其他 [cite: 3, 4] |
| data[].taskInfos[] | serviceName | String | [cite_start]算法组件名称 [cite: 4] |
| data[].taskInfos[] | serviceUUID | String | [cite_start]算法组件唯一标识 [cite: 4] |
| data[].taskInfos[] | stackName | String | [cite_start]业务链路名称 [cite: 4] |
| data[].taskInfos[] | stackUUID | String | [cite_start]业务链路唯一标识 [cite: 4] |

---

## 2. 根据业务链路标签批量启用业务链路

### 2.1 需求描述
[cite_start]传入业务链路标签UUID，一次性启动所有含有该标签的当前版本业务链路 [cite: 4]。

### 2.2 接口基本信息
* **接口名称**：根据业务链路标签批量启用业务链路
* **请求方式**：`POST`
* **请求地址**：`http://api/v1/stacks/labels/deploy`
* **Content-Type**：`application/json`

### 2.3 请求体 (Body)

| 参数名 | 必填 | 类型 | 示例值 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| stackLabels | 是 | List\<String\> | `["label1","label2"]` | [cite_start]字符串类型列表 [cite: 4] |
| stop | 否 | Integer | `0` | 1: 先停止其他业务再启动本服务; [cite_start]0: 启动服务，不排他 [cite: 4] |
| account | 是 | String | `admin` | [cite_start]用户账号 [cite: 4] |
| password | 是 | String | `12q12w12ee` | [cite_start]密码 [cite: 4] |

### 2.4 响应体 (Response)

| 字段层级 | 字段名 | 类型 | 说明 |
| :--- | :--- | :--- | :--- |
| Root | code | Integer | 响应码 |
| Root | message | String | 响应信息 |
| data[].successStackInfos[] | stackName | String | [cite_start]启用成功业务链路名称 [cite: 5] |
| data[].successStackInfos[] | stackUUID | String | [cite_start]启用成功业务链路UUID [cite: 5] |
| data[].successStackInfos[] | message | String | [cite_start]详细信息 [cite: 6] |
| data[].failureStackInfos[] | stackName | String | [cite_start]启用失败业务链路名称 [cite: 7] |
| data[].failureStackInfos[] | stackUUID | String | [cite_start]启用失败业务链路UUID [cite: 7] |
| data[].failureStackInfos[] | message | String | [cite_start]启用失败信息 [cite: 7] |

---

## 3. 根据业务链路标签批量停用业务链路

### 3.1 需求描述
[cite_start]传入业务链路标签UUID，一次性停用所有含有该标签的当前版本业务链路 [cite: 8]。

### 3.2 接口基本信息
* **接口名称**：根据业务链路标签批量停用业务链路
* **请求方式**：`POST`
* **请求地址**：`http://api/v1/stacks/labels/undeploy`
* **Content-Type**：`application/json`

### 3.3 请求体 (Body)

| 参数名 | 必填 | 类型 | 示例值 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| stackLabels | 是 | List\<String\> | `["label1","label2"]` | [cite_start]字符串类型列表 [cite: 8] |

### 3.4 响应体 (Response)

| 字段层级 | 字段名 | 类型 | 说明 |
| :--- | :--- | :--- | :--- |
| Root | code | Integer | 响应码 |
| Root | message | String | 响应信息 |
| data[].successStackInfos[] | stackName | String | [cite_start]停用成功业务链路名称 [cite: 9] |
| data[].successStackInfos[] | stackUUID | String | [cite_start]停用成功业务链路UUID [cite: 9] |
| data[].successStackInfos[] | message | String | [cite_start]详情信息 [cite: 10] |
| data[].failureStackInfos[] | stackName | String | [cite_start]停用失败业务链路名称 [cite: 11] |
| data[].failureStackInfos[] | stackUUID | String | [cite_start]停用失败业务链路UUID [cite: 11] |
| data[].failureStackInfos[] | message | String | [cite_start]停用失败详情信息 [cite: 11] |

---

## 4. 获取所有业务链路信息和状态 (业务链路详情)

### 4.1 需求描述
[cite_start]获取所有业务链路信息（含启动状态）、组件信息（ID、名称、任务ID）、运行状态（板卡位置）以及资源使用情况（CPU、内存、网络、GPU等） [cite: 12]。

### 4.2 接口基本信息
* **接口名称**：获取所有业务链路信息和状态
* **请求方式**：`POST`
* **请求地址**：`http://api/v1/external/qyw/stackinfo`
* **Content-Type**：`application/json`
* **认证方式**：Bearer Token（待定）

### 4.3 响应体 (Response)

| 字段层级 | 字段名 | 类型 | 说明 |
| :--- | :--- | :--- | :--- |
| Root | code | Integer | 响应码 |
| Root | message | String | 响应信息 |
| data[] | stackName | String | [cite_start]业务链路名称 [cite: 13] |
| data[] | stackUUID | String | [cite_start]业务链路UUID [cite: 13] |
| data[].stackLabelInfos[] | stackLabelName | String | [cite_start]业务链路标签名称 [cite: 13] |
| data[].stackLabelInfos[] | stackLabelUUID | String | [cite_start]业务链路标签UUID [cite: 13] |
| data[] | stackDeployStatus | Integer | [cite_start]部署状态：0-未部署；1-已部署 [cite: 14] |
| data[] | stackRunningStatus | Integer | [cite_start]运行状态：1-正常运行；2-异常运行 [cite: 14] |
| data[].serviceInfos[] | serviceName | String | [cite_start]算法组件名称 [cite: 14] |
| data[].serviceInfos[] | serviceUUID | String | [cite_start]算法组件UUID [cite: 15] |
| data[].serviceInfos[] | serviceStatus | Integer | [cite_start]组件状态：0-已停用；1-已启用；2-运行正常；3-运行异常 [cite: 15] |
| data[].serviceInfos[] | serviceType | Integer | [cite_start]组件类型：0-普通；1-公共链路引用；2-公共链路自有 [cite: 15] |
| data[].serviceInfos[].taskInfos[] | taskID | String | [cite_start]任务ID [cite: 16] |
| data[].serviceInfos[].taskInfos[] | taskStatus | Integer | [cite_start]任务状态：1-运行中, 2-已完成, 3-异常, 0-其他 [cite: 16] |
| data[].serviceInfos[].taskInfos[] | cpuCores | Float | [cite_start]CPU总量 [cite: 16] |
| data[].serviceInfos[].taskInfos[] | cpuUsed | Float | [cite_start]CPU使用量 [cite: 16] |
| data[].serviceInfos[].taskInfos[] | cpuUsage | Float | [cite_start]CPU使用率 [cite: 16] |
| data[].serviceInfos[].taskInfos[] | memorySize | Float | [cite_start]内存总量 [cite: 16] |
| data[].serviceInfos[].taskInfos[] | memoryUsed | Float | [cite_start]内存使用量 [cite: 16] |
| data[].serviceInfos[].taskInfos[] | memoryUsage | Float | [cite_start]内存使用率 [cite: 16] |
| data[].serviceInfos[].taskInfos[] | netReceive | Float | [cite_start]网络接收流量 [cite: 17] |
| data[].serviceInfos[].taskInfos[] | netSent | Float | [cite_start]网络发送流量 [cite: 17] |
| data[].serviceInfos[].taskInfos[] | gpuMemUsed | Float | [cite_start]GPU显存使用情况 [cite: 17] |
| data[].serviceInfos[].taskInfos[] | chassisName | String | [cite_start]机箱名称 [cite: 17] |
| data[].serviceInfos[].taskInfos[] | chassisNumber | Integer | [cite_start]机箱号 [cite: 17] |
| data[].serviceInfos[].taskInfos[] | boardName | String | [cite_start]板卡名称 [cite: 17] |
| data[].serviceInfos[].taskInfos[] | boardNumber | Integer | [cite_start]板卡槽位号 [cite: 18] |
| data[].serviceInfos[].taskInfos[] | boardAddress | String | [cite_start]板卡IP地址 [cite: 18] |

---

## 5. 板卡异常上报

### 5.1 需求描述
[cite_start]当板卡状态异常（如离线）时，主动发送告警信息 [cite: 19]。

### 5.2 接口基本信息
* **接口名称**：板卡异常上报
* **请求方式**：`POST`
* **请求地址**：715内部接口
* **Content-Type**：`application/json`

### 5.3 请求体 (Body)

| 字段名 | 类型 | 必填 | 说明 |
| :--- | :--- | :--- | :--- |
| chassisName | String | 是 | [cite_start]机箱名称 [cite: 19] |
| chassisNumber | Integer | 是 | [cite_start]机箱号 [cite: 19] |
| boardName | String | 是 | [cite_start]板卡名称 [cite: 19] |
| boardNumber | Integer | 是 | [cite_start]板卡槽位号 [cite: 20] |
| boardType | Integer | 是 | [cite_start]板卡类型 [cite: 20] |
| boardAddress | String | 是 | [cite_start]板卡IP地址 [cite: 20] |
| boardStatus | Integer | 是 | [cite_start]板卡状态：0-正常，1-异常 [cite: 20] |
| alertMessages | List\<String\> | 是 | [cite_start]板卡告警信息列表 [cite: 20] |

### 5.4 响应体 (Response)

| 字段名 | 类型 | 说明 |
| :--- | :--- | :--- |
| code | Integer | 响应码 (0表示成功) |
| message | String | 响应信息 |
| data | String | 数据负载 ("success") |

---

## 6. 组件异常上报

### 6.1 需求描述
[cite_start]当组件运行异常时，主动发送告警信息（包含任务ID、运行状态、所属链路、所在板卡位置等） [cite: 21]。

### 6.2 接口基本信息
* **接口名称**：组件异常上报
* **请求方式**：`POST`
* **请求地址**：715内部接口
* **Content-Type**：`application/json`

### 6.3 请求体 (Body)

| 字段名 | 类型 | 必填 | 说明 |
| :--- | :--- | :--- | :--- |
| stackName | String | 是 | [cite_start]业务链路名称 [cite: 21] |
| stackUUID | String | 是 | [cite_start]业务链路UUID [cite: 21] |
| serviceName | String | 是 | [cite_start]算法组件名称 [cite: 21] |
| serviceUUID | String | 是 | [cite_start]算法组件UUID [cite: 22] |
| taskAlertInfos[].taskID | String | 是 | [cite_start]任务ID [cite: 22] |
| taskAlertInfos[].taskStatus | Integer | 是 | [cite_start]任务状态：1-运行中, 2-已完成, 3-异常, 0-其他 [cite: 22] |
| taskAlertInfos[].chassisName | String | 是 | [cite_start]机箱名称 [cite: 22] |
| taskAlertInfos[].chassisNumber | Integer | 是 | [cite_start]机箱号 [cite: 23] |
| taskAlertInfos[].boardName | String | 是 | [cite_start]板卡名称 [cite: 23] |
| taskAlertInfos[].boardNumber | Integer | 是 | [cite_start]板卡槽位号 [cite: 23] |
| taskAlertInfos[].boardType | Integer | 是 | [cite_start]板卡类型 [cite: 23] |
| taskAlertInfos[].boardAddress | String | 是 | [cite_start]板卡IP地址 [cite: 24] |
| taskAlertInfos[].boardStatus | Integer | 是 | [cite_start]板卡状态：0-正常，1-异常 [cite: 24] |
| taskAlertInfos[].alertMessages | List\<String\> | 是 | [cite_start]组件告警信息列表 [cite: 24] |

### 6.4 响应体 (Response)

| 字段名 | 类型 | 说明 |
| :--- | :--- | :--- |
| code | Integer | 响应码 (0表示成功) |
| message | String | 响应信息 |
| data | String | 数据负载 ("success") |

---

## 7. IP心跳检测接口

### 7.1 需求描述
[cite_start]客户端上报IP地址，软件框架利用此IP调用客户端接口推送异常信息 [cite: 25]。

### 7.2 接口基本信息
* **接口名称**：IP心跳检测
* **请求方式**：`GET`
* **请求地址**：715内部接口 (示例: `api/v1/sys-config/client/up`)

### 7.3 请求参数 (Query)

| 参数名 | 类型 | 必填 | 示例值 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| clientIp | String | 是 | 192.168.6.222 | [cite_start]客户端IP地址 [cite: 25] |

### 7.4 响应体 (Response)

| 字段名 | 类型 | 说明 |
| :--- | :--- | :--- |
| code | Integer | 响应码 (0表示成功) |
| message | String | 响应信息 |
| data | String | 数据负载 ("success") |

---

## 8. 业务链路复位接口

### 8.1 需求描述
停止当前所有业务链路。

### 8.2 接口基本信息
* **接口名称**：业务链路复位接口
* **请求方式**：`GET`
* **请求地址**：`http://api/v1/stacks/labels/reset`

### 8.3 请求参数
* 无

### 8.4 响应体 (Response)

| 字段名 | 类型 | 说明 |
| :--- | :--- | :--- |
| code | Integer | 响应码 (0表示成功) |
| message | String | 响应信息 |
| data | String | 数据负载 ("success") |

---

## [cite_start]附录：板卡类型代码表 [cite: 25]

| 类型代码 | 说明 |
| :--- | :--- |
| 0 | 其他 |
| 1 | CPU通用计算模块A型 |
| 2 | CPU通用计算模块B型 |
| 3 | GPU I型高性能计算模块 |
| 4 | GPU II型高性能计算模块 |
| 5 | 综合计算模块A型 |
| 6 | 综合计算模块B型 |