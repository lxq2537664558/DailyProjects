((lambda (Y for-each fib)
   ((Y for-each) (lambda (i) ((Y fib) 0 1 0 i)) 0 40)
   )
 (lambda (f)
   ((lambda (Y) (Y Y))
    (lambda (Y-self)
      (f (lambda (a b c d e) ((Y-self Y-self) a b c d e))))))
 (lambda (self)
   (lambda (f i n)
     (print (f i))
     ((= i n)
      (lambda () i)
      (lambda () (self f (+ i 1) n)))))
 (lambda (self)
   (lambda (a b i n)
     ((= i n)
      (lambda () a)
      (lambda () (self b (+ a b) (+ i 1) n))))))