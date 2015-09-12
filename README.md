# excel-report

Utils for STP System

```
npm install stp-utils
```

## Usage

``` js
var stp = require('stp-utils')

var tb =[
	{ma_so:'A',gia_tri:10,gia_tri_2:4},
	{ma_so:'C',gia_tri:2,gia_tri_2:8},
	{ma_so:'D',cong_thuc:"[A] + [B]"},
	{ma_so:'B',cong_thuc:"[C] + [A]+[Z]"},
	{ma_so:'Z',cong_thuc:"[A]+[M]"}
]
stp.calcGrid(tb,"gia_tri,gia_tri_2",function(data){
	console.log(data);
})
```
## License

MIT
